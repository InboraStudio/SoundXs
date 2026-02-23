#include "AudioEngine.h"

#include <algorithm>
#include <audioclient.h>
#include <avrt.h>
#include <cmath>
#include <cstring>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <windows.h>

typedef enum PROCESS_LOOPBACK_MODE {
  PROCESS_LOOPBACK_MODE_INCLUDE = 0,
  PROCESS_LOOPBACK_MODE_EXCLUDE = 1
} PROCESS_LOOPBACK_MODE;

typedef struct AUDCLNT_PROCESS_LOOPBACK_PARAMS {
  DWORD TargetProcessId;
  PROCESS_LOOPBACK_MODE ProcessLoopbackMode;
} AUDCLNT_PROCESS_LOOPBACK_PARAMS;

AudioRingBuffer::AudioRingBuffer(size_t capacityPow2)
    : m_buf(capacityPow2, 0.0f), m_mask(capacityPow2 - 1) {}

size_t AudioRingBuffer::write(const float *src, size_t count) noexcept {
  const size_t wPos = m_writePos.load(std::memory_order_relaxed);
  const size_t rPos = m_readPos.load(std::memory_order_acquire);
  const size_t cap = m_mask + 1;
  const size_t space = cap - (wPos - rPos);

  if (count > space) {
    m_readPos.store(wPos + count - cap, std::memory_order_release);
  }

  const size_t n = std::min(count, cap);
  for (size_t i = 0; i < n; ++i)
    m_buf[(wPos + i) & m_mask] = src[i];
  m_writePos.store(wPos + n, std::memory_order_release);
  return n;
}

size_t AudioRingBuffer::read(float *dst, size_t count) noexcept {
  const size_t rPos = m_readPos.load(std::memory_order_relaxed);
  const size_t wPos = m_writePos.load(std::memory_order_acquire);
  const size_t avail = wPos - rPos;
  const size_t n = std::min(count, avail);
  for (size_t i = 0; i < n; ++i)
    dst[i] = m_buf[(rPos + i) & m_mask];
  m_readPos.store(rPos + n, std::memory_order_release);
  return n;
}

size_t AudioRingBuffer::available() const noexcept {
  return m_writePos.load(std::memory_order_acquire) -
         m_readPos.load(std::memory_order_acquire);
}

void AudioRingBuffer::reset() noexcept {
  m_readPos.store(0, std::memory_order_relaxed);
  m_writePos.store(0, std::memory_order_relaxed);
}

template <typename T> static void safeRelease(T *&p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

static WAVEFORMATEXTENSIBLE makeFloat32Format() {
  WAVEFORMATEXTENSIBLE wfex{};
  wfex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wfex.Format.nChannels = 2;
  wfex.Format.nSamplesPerSec = 48000;
  wfex.Format.wBitsPerSample = 32;
  wfex.Format.nBlockAlign =
      wfex.Format.nChannels * wfex.Format.wBitsPerSample / 8;
  wfex.Format.nAvgBytesPerSec =
      wfex.Format.nSamplesPerSec * wfex.Format.nBlockAlign;
  wfex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  wfex.Samples.wValidBitsPerSample = 32;
  wfex.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
  wfex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
  return wfex;
}

AudioEngine::AudioEngine(QObject *parent) : QObject(parent) {
  CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                   __uuidof(IMMDeviceEnumerator),
                   reinterpret_cast<void **>(&m_enumerator));
}

AudioEngine::~AudioEngine() {
  stop();
  safeRelease(m_enumerator);
}

bool AudioEngine::openRenderDevice(const QString &id) {
  if (!m_enumerator)
    return false;
  HRESULT hr;
  if (id.isEmpty())
    hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                               &m_renderDevice);
  else
    hr = m_enumerator->GetDevice(id.toStdWString().c_str(), &m_renderDevice);
  if (FAILED(hr))
    return false;

  hr = m_renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void **>(&m_renderClient));
  if (FAILED(hr))
    return false;

  WAVEFORMATEXTENSIBLE wfex = makeFloat32Format();
  hr = m_renderClient->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
          AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
      200000, 0, reinterpret_cast<WAVEFORMATEX *>(&wfex), nullptr);
  if (FAILED(hr))
    return false;

  m_renderEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  m_renderClient->SetEventHandle(m_renderEvent);
  m_renderClient->GetService(__uuidof(IAudioRenderClient),
                             reinterpret_cast<void **>(&m_renderInterface));
  m_dsp.setSampleRate(48000.0);
  return true;
}

bool AudioEngine::openCaptureDevice(const QString &id) {
  if (!m_enumerator)
    return false;
  HRESULT hr;
  if (id.isEmpty())
    hr = m_enumerator->GetDefaultAudioEndpoint(eCapture, eConsole,
                                               &m_captureDevice);
  else
    hr = m_enumerator->GetDevice(id.toStdWString().c_str(), &m_captureDevice);
  if (FAILED(hr))
    return false;

  hr = m_captureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void **>(&m_captureClient));
  if (FAILED(hr))
    return false;

  WAVEFORMATEXTENSIBLE wfex = makeFloat32Format();
  hr = m_captureClient->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
          AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
      200000, 0, reinterpret_cast<WAVEFORMATEX *>(&wfex), nullptr);
  if (FAILED(hr))
    return false;

  m_captureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  m_captureClient->SetEventHandle(m_captureEvent);
  m_captureClient->GetService(__uuidof(IAudioCaptureClient),
                              reinterpret_cast<void **>(&m_captureInterface));
  return true;
}

bool AudioEngine::openLoopbackDevice(uint32_t pid) {
  if (!m_enumerator || !m_loopbackEnabled.load())
    return false;
  HRESULT hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                     &m_loopbackDevice);
  if (FAILED(hr))
    return false;

  hr = m_loopbackDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void **>(&m_loopbackClient));
  if (FAILED(hr))
    return false;

  WAVEFORMATEXTENSIBLE wfex = makeFloat32Format();
  DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                AUDCLNT_STREAMFLAGS_LOOPBACK |
                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

  AUDCLNT_PROCESS_LOOPBACK_PARAMS params{};
  void *pReserved = nullptr;
  if (pid != 0) {
    params.TargetProcessId = pid;
    params.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE;
    pReserved = &params;
  }

  hr = m_loopbackClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 200000, 0,
                                    reinterpret_cast<WAVEFORMATEX *>(&wfex),
                                    (LPCGUID)pReserved);
  if (FAILED(hr))
    return false;

  m_loopbackEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  m_loopbackClient->SetEventHandle(m_loopbackEvent);
  m_loopbackClient->GetService(__uuidof(IAudioCaptureClient),
                               reinterpret_cast<void **>(&m_loopbackInterface));
  return true;
}

void AudioEngine::closeCaptureDevice() {
  if (m_captureClient)
    m_captureClient->Stop();
  safeRelease(m_captureInterface);
  safeRelease(m_captureClient);
  safeRelease(m_captureDevice);
  if (m_captureEvent) {
    CloseHandle(m_captureEvent);
    m_captureEvent = nullptr;
  }
}

void AudioEngine::closeRenderDevice() {
  if (m_renderClient)
    m_renderClient->Stop();
  safeRelease(m_renderInterface);
  safeRelease(m_renderClient);
  safeRelease(m_renderDevice);
  if (m_renderEvent) {
    CloseHandle(m_renderEvent);
    m_renderEvent = nullptr;
  }
}

void AudioEngine::closeLoopbackDevice() {
  if (m_loopbackClient)
    m_loopbackClient->Stop();
  safeRelease(m_loopbackInterface);
  safeRelease(m_loopbackClient);
  safeRelease(m_loopbackDevice);
  if (m_loopbackEvent) {
    CloseHandle(m_loopbackEvent);
    m_loopbackEvent = nullptr;
  }
}

bool AudioEngine::start(const QString &inputId, const QString &outputId) {
  std::lock_guard<std::mutex> lk(m_restartMutex);
  stop();
  m_currentInputId = inputId;
  m_currentOutputId = outputId;
  m_ringBuffer.reset();
  m_loopbackRingBuffer.reset();

  if (!openCaptureDevice(inputId) || !openRenderDevice(outputId))
    return false;
  bool loopOk = openLoopbackDevice(m_loopbackPid.load());

  m_running.store(true);
  m_captureClient->Start();
  m_renderClient->Start();
  if (loopOk)
    m_loopbackClient->Start();

  m_captureThread = std::thread(&AudioEngine::captureThreadFunc, this);
  m_renderThread = std::thread(&AudioEngine::renderThreadFunc, this);
  if (loopOk)
    m_loopbackThread = std::thread(&AudioEngine::loopbackThreadFunc, this);
  return true;
}

void AudioEngine::stop() {
  if (!m_running.load())
    return;
  m_running.store(false);
  if (m_captureEvent)
    SetEvent(m_captureEvent);
  if (m_renderEvent)
    SetEvent(m_renderEvent);
  if (m_loopbackEvent)
    SetEvent(m_loopbackEvent);
  if (m_captureThread.joinable())
    m_captureThread.join();
  if (m_renderThread.joinable())
    m_renderThread.join();
  if (m_loopbackThread.joinable())
    m_loopbackThread.join();
  closeCaptureDevice();
  closeRenderDevice();
  closeLoopbackDevice();
}

void AudioEngine::setInputDevice(const QString &id) {
  start(id, m_currentOutputId);
}
void AudioEngine::setOutputDevice(const QString &id) {
  start(m_currentInputId, id);
}

void AudioEngine::setLoopbackEnabled(bool enabled, uint32_t pid) {
  if (m_loopbackEnabled.load() == enabled && m_loopbackPid.load() == pid)
    return;
  m_loopbackEnabled.store(enabled);
  m_loopbackPid.store(pid);
  if (m_running.load())
    start(m_currentInputId, m_currentOutputId);
}

void AudioEngine::captureThreadFunc() {
  while (m_running.load(std::memory_order_relaxed)) {
    if (WaitForSingleObject(m_captureEvent, 50) != WAIT_OBJECT_0)
      continue;
    BYTE *data = nullptr;
    UINT32 frames = 0;
    DWORD flags = 0;
    while (SUCCEEDED(m_captureInterface->GetBuffer(&data, &frames, &flags,
                                                   nullptr, nullptr)) &&
           frames > 0) {
      if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data)
        m_ringBuffer.write((const float *)data,
                           (size_t)frames * TARGET_CHANNELS);
      m_captureInterface->ReleaseBuffer(frames);
    }
  }
}

void AudioEngine::loopbackThreadFunc() {
  while (m_running.load(std::memory_order_relaxed)) {
    if (WaitForSingleObject(m_loopbackEvent, 50) != WAIT_OBJECT_0)
      continue;
    BYTE *data = nullptr;
    UINT32 frames = 0;
    DWORD flags = 0;
    while (SUCCEEDED(m_loopbackInterface->GetBuffer(&data, &frames, &flags,
                                                    nullptr, nullptr)) &&
           frames > 0) {
      if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data)
        m_loopbackRingBuffer.write((const float *)data,
                                   (size_t)frames * TARGET_CHANNELS);
      m_loopbackInterface->ReleaseBuffer(frames);
    }
  }
}

void AudioEngine::renderThreadFunc() {
  UINT32 bufferSize = 0;
  m_renderClient->GetBufferSize(&bufferSize);
  std::vector<float> loopBuf;
  while (m_running.load(std::memory_order_relaxed)) {
    if (WaitForSingleObject(m_renderEvent, 50) != WAIT_OBJECT_0)
      continue;
    UINT32 padding = 0;
    m_renderClient->GetCurrentPadding(&padding);
    UINT32 available = bufferSize - padding;
    if (available == 0)
      continue;
    BYTE *renderBuf = nullptr;
    if (FAILED(m_renderInterface->GetBuffer(available, &renderBuf)))
      break;
    float *fBuf = (float *)renderBuf;
    size_t need = (size_t)available * TARGET_CHANNELS;
    size_t got = m_ringBuffer.read(fBuf, need);
    if (got < need)
      std::memset(fBuf + got, 0, (need - got) * sizeof(float));
    if (m_loopbackEnabled.load()) {
      if (loopBuf.size() < need)
        loopBuf.resize(need);
      size_t gotLoop = m_loopbackRingBuffer.read(loopBuf.data(), need);
      for (size_t i = 0; i < gotLoop; ++i)
        fBuf[i] += loopBuf[i] * 0.7f;
    }
    m_dsp.process(fBuf, (int)available, TARGET_CHANNELS);
    for (size_t i = 0; i < need; ++i) { // Limiter
      if (fBuf[i] > 0.99f)
        fBuf[i] = 0.99f;
      else if (fBuf[i] < -0.99f)
        fBuf[i] = -0.99f;
    }
    float peak = 0.0f;
    for (size_t i = 0; i < need; ++i)
      peak = std::max(peak, std::abs(fBuf[i]));
    m_peakLevel.store(m_peakLevel.load() * 0.9f + peak * 0.1f,
                      std::memory_order_relaxed);
    m_renderInterface->ReleaseBuffer(available, 0);
  }
}

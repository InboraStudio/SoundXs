#pragma once
#include <QObject>
#include <QString>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>


#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>


#include "DSPEngine.h"
class AudioRingBuffer {
public:
  explicit AudioRingBuffer(size_t capacityPow2);

  size_t write(const float *src, size_t count) noexcept;
  size_t read(float *dst, size_t count) noexcept;
  size_t available() const noexcept;
  void reset() noexcept;

private:
  std::vector<float> m_buf;
  size_t m_mask;
  std::atomic<size_t> m_writePos{0};
  std::atomic<size_t> m_readPos{0};
};
class AudioEngine : public QObject {
  Q_OBJECT
public:
  explicit AudioEngine(QObject *parent = nullptr);
  ~AudioEngine() override;

  bool start(const QString &inputDeviceId, const QString &outputDeviceId);
  void stop();
  bool isRunning() const noexcept { return m_running.load(); }

  void setInputDevice(const QString &id);
  void setOutputDevice(const QString &id);
  void setLoopbackEnabled(bool enabled, uint32_t pid = 0);
  bool isLoopbackEnabled() const noexcept { return m_loopbackEnabled.load(); }

  DSPEngine *dsp() noexcept { return &m_dsp; }

  float peakLevel() const noexcept { return m_peakLevel.load(); }

signals:
  void engineError(const QString &message);

private:
  void captureThreadFunc();
  void loopbackThreadFunc();
  void renderThreadFunc();

  bool openCaptureDevice(const QString &id);
  bool openRenderDevice(const QString &id);
  bool openLoopbackDevice(uint32_t pid);
  void closeCaptureDevice();
  void closeRenderDevice();
  void closeLoopbackDevice();

  IMMDeviceEnumerator *m_enumerator = nullptr;


  IMMDevice *m_captureDevice = nullptr;
  IAudioClient *m_captureClient = nullptr;
  IAudioCaptureClient *m_captureInterface = nullptr;
  HANDLE m_captureEvent = nullptr;

  IMMDevice *m_loopbackDevice = nullptr;
  IAudioClient *m_loopbackClient = nullptr;
  IAudioCaptureClient *m_loopbackInterface = nullptr;
  HANDLE m_loopbackEvent = nullptr;

  IMMDevice *m_renderDevice = nullptr;
  IAudioClient *m_renderClient = nullptr;
  IAudioRenderClient *m_renderInterface = nullptr;
  HANDLE m_renderEvent = nullptr;

  static constexpr UINT32 TARGET_SAMPLERATE = 48000;
  static constexpr UINT16 TARGET_CHANNELS = 2;

  std::thread m_captureThread;
  std::thread m_loopbackThread;
  std::thread m_renderThread;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_loopbackEnabled{false};
  std::atomic<uint32_t> m_loopbackPid{0};

  QString m_currentInputId;
  QString m_currentOutputId;
  std::mutex m_restartMutex;

  
  AudioRingBuffer m_ringBuffer{1u << 17};        
  AudioRingBuffer m_loopbackRingBuffer{1u << 17}; 

  DSPEngine m_dsp;
  std::atomic<float> m_peakLevel{0.0f};
};

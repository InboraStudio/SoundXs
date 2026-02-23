<div align="center">

<h1>🎛️ SoundXs</h1>

<p><strong>Precision Tone. Zero Noise. Zero Compromise.</strong></p>

<p>
  <a href="https://github.com/InboraStudio/SoundXs/releases">
    <img src="https://img.shields.io/github/v/release/InboraStudio/SoundXs?style=for-the-badge&color=00bfff" alt="Release">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?style=for-the-badge&logo=windows" alt="Windows 10/11">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/Built%20With-C%2B%2B20-blue?style=for-the-badge&logo=cplusplus" alt="C++20">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/Qt-6.x-41CD52?style=for-the-badge&logo=qt" alt="Qt6">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/Audio%20API-WASAPI-purple?style=for-the-badge" alt="WASAPI">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/License-MIT-orange?style=for-the-badge" alt="MIT License">
  </a>
</p>

<img width="1050" height="600" alt="SoundXs" src="https://github.com/user-attachments/assets/84e49329-16d3-47a2-8f81-c6ebef2f42c5" />



<p><em>A professional-grade, real-time audio processing engine for Windows 10/11.<br>
Built from the ground up with WASAPI, hand-crafted RBJ biquad DSP filters, a lock-free SPSC ring buffer, and a premium dark Qt 6 UI.</em></p>


<br/>

> **By [Inbora Studio](https://github.com/InboraStudio)** &nbsp;•&nbsp; **Developed by drchamyoung**

</div>

## What Is SoundXs?

SoundXs is a **real-time system-wide audio tone processor** for Windows. It sits transparently between your microphone (or any capture device) and your speakers/headphones, running a full professional-grade DSP chain on every single audio frame — **in under 20 milliseconds**.

Unlike software equalizers that hook into audio drivers at a high level, SoundXs works directly at the **WASAPI (Windows Audio Session API)** layer — the same low-level interface used by professional DAWs like Ableton and FL Studio. This gives it extremely precise control over audio timing, format, and routing.

```
Microphone / Capture Device
        │
        ▼  [WASAPI Shared Mode Capture]
 ┌──────────────────────────┐
 │   Capture Thread         │  ← IAudioCaptureClient, event-driven
 │   (PCM float32 data)     │
 └──────────┬───────────────┘
            │  writes into
            ▼
 ┌──────────────────────────┐
 │   Lock-Free Ring Buffer  │  ← 131,072 floats (~1.37 s stereo 48 kHz)
 └──────────┬───────────────┘
            │  reads from
            ▼
 ┌──────────────────────────┐
 │   Render Thread          │  ← IAudioRenderClient, event-driven
 │   + Desktop Audio Mix    │  ← loopback ring buffer (0.7× gain)
 │   + DSPEngine::process() │  ← Echo → 4× Biquad EQ → Comp → Limiter
 └──────────┬───────────────┘
            │
            ▼  [WASAPI Shared Mode Render]
    Speakers / Headphones
```

---

## Feature Matrix

| Feature | Details |
|---|---|
| **Bass Control** | RBJ Low-Shelf @ 80 Hz, ±12 dB, Q = √2 |
| **Treble Control** | RBJ High-Shelf @ 10 kHz, ±12 dB, Q = √2 |
| **Warmth Control** | RBJ Peaking EQ @ 220 Hz, Q = 1.0, ±12 dB |
| **Presence Control** | RBJ Peaking EQ @ 2,800 Hz, Q = 1.0, ±12 dB |
| **Master Volume** | Linear gain from dB, −20 to +10 dB |
| **Compressor** | Feed-forward peak compressor, 4:1 ratio, 5 ms attack / 150 ms release |
| **Echo / Delay** | Circular delay buffer, 500 ms, 50% feedback, 0–100% wet mix |
| **Soft Limiter** | `tanh(x)` saturation at ±0.95 / hard clip at ±0.99 |
| **Input Device** | Any WASAPI-compatible capture endpoint, hot-swappable |
| **Output Device** | Any WASAPI-compatible render endpoint, hot-swappable |
| **Desktop Loopback** | Capture any app's audio by PID (Windows 10 2004+ process loopback) |
| **Peak Meter** | Exponentially-smoothed peak display (10% new / 90% old) |
| **Presets** | Flat, Bass Boost, Treble Boost; all parameters persisted via `QSettings` |
| **Dark UI** | Qt 6, ~60 fps, custom QSS stylesheet, premium dark industrial theme |

---

## System Architecture

### Thread Model

SoundXs uses **three dedicated real-time threads** alongside the main Qt UI thread. Every thread runs at `THREAD_PRIORITY_TIME_CRITICAL` with MMCSS ("Pro Audio" task) scheduling to prevent OS preemption.

```
┌──────────────────────────────────────────────────────────────────┐
│   OS / Windows Scheduler                                         │
│                                                                  │
│  ┌────────────┐  ┌─────────────────────┐  ┌────────────────────┐│
│  │  UI Thread │  │  Capture Thread      │  │  Render Thread     ││
│  │  (Qt 6)    │  │  THREAD_PRIORITY     │  │  THREAD_PRIORITY   ││
│  │            │  │  _TIME_CRITICAL      │  │  _TIME_CRITICAL    ││
│  │  Slider ──►│  │  MMCSS "Pro Audio"   │  │  MMCSS "Pro Audio" ││
│  │  SeqLock   │  │                      │  │                    ││
│  │  .store()  │  │  WaitForSingleObject │  │  WaitForSingleObj  ││
│  └─────┬──────┘  └──────────┬──────────┘  └────────┬───────────┘│
│        │                    │                       │            │
│  SeqLock write          write()                  read()          │
│        ▼                    ▼                       ▼            │
│     DspParams ──────► AudioRingBuffer ──────► DSPEngine::process │
└──────────────────────────────────────────────────────────────────┘
```

An optional **Loopback Thread** is spawned at the same priority when desktop audio capture is active.

### Data Flow Diagram

```
[User moves Bass slider]
        │
        │  UI Thread calls DSPEngine::setBass(dB)
        ▼
  SeqLock<DspParams>::store(newParams)    ← atomic write, no lock
        │
        │  audio thread polls ~100 Hz
        ▼
  SeqLock<DspParams>::load()             ← wait-free read, lockless
        │
        │  updateCoeffsIfNeeded()
        │  if (bassChanged):
        ▼
  makeLowShelf(sr=48000, f0=80Hz, dB)   ← recomputes 5 doubles
        │
        ▼
  m_bassCoeffs updated — new sound starts on the very next frame
```

---

## Audio Engine Deep Dive

### WASAPI Integration

SoundXs uses **shared-mode event-driven WASAPI** with these stream flags:

```cpp
AUDCLNT_STREAMFLAGS_EVENTCALLBACK     // event-driven: OS signals when buffer is ready
| AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM  // Windows handles sample-rate conversion
| AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY  // high-quality sinc SRC
```

The audio format is always negotiated as:

| Parameter | Value |
|---|---|
| Format Tag | `WAVE_FORMAT_EXTENSIBLE` |
| Channels | 2 (Stereo) |
| Sample Rate | 48,000 Hz |
| Bit Depth | 32-bit IEEE 754 float |
| Block Align | 8 bytes (2 ch × 4 bytes) |
| Avg Bytes/Sec | 384,000 |
| SubFormat | `KSDATAFORMAT_SUBTYPE_IEEE_FLOAT` |
| Channel Mask | `KSAUDIO_SPEAKER_STEREO` (FL + FR) |

The engine requests a **200,000 × 100 ns = 20 ms** hardware buffer. On hot-swap, `AudioEngine::start()` runs under a `std::mutex` restart lock that signals all events, joins all threads, releases all COM interfaces, then re-opens the new device and re-spawns threads.

---

### Lock-Free SPSC Ring Buffer

The ring buffer bridges the capture thread (producer) and render thread (consumer). It is a **Single-Producer Single-Consumer (SPSC)** lock-free queue built on two `std::atomic<size_t>` cursors.

```
 capacity = 2^17 = 131,072 floats = 65,536 stereo frames
 mask     = capacity - 1    ← bitwise AND replaces modulo

 m_buf:  [f0][f1][f2]...[f131071]
          ↑                   ↑
        rPos               wPos
```

**Write path (capture thread):**

```cpp
space = cap - (wPos - rPos)              // available slots
if (count > space)
    rPos = wPos + count - cap            // drop oldest data to prevent stutter
for i in [0, n):
    buf[(wPos + i) & mask] = src[i]     // bitwise modulo indexing
wPos.store(wPos + n, release)           // publish to consumer
```

**Read path (render thread):**

```cpp
avail = wPos - rPos
n     = min(count, avail)
for i in [0, n):
    dst[i] = buf[(rPos + i) & mask]
rPos.store(rPos + n, release)
```

The `release/acquire` pairing ensures the CPU store-buffer is flushed before the pointer update is visible — **no mutex, no explicit memory barrier** on x86-64 (TSO).

---

### Desktop Loopback Capture

Pressing **"Capture Desktop Audio"** opens a third WASAPI stream on the default render endpoint with `AUDCLNT_STREAMFLAGS_LOOPBACK`, intercepting all PCM data flowing to your speakers.

On **Windows 10 2004+**, SoundXs uses `AUDCLNT_PROCESS_LOOPBACK_PARAMS` to filter capture to a **single selected process**:

```cpp
typedef struct AUDCLNT_PROCESS_LOOPBACK_PARAMS {
    DWORD                 TargetProcessId;
    PROCESS_LOOPBACK_MODE ProcessLoopbackMode; // INCLUDE = only this PID
} AUDCLNT_PROCESS_LOOPBACK_PARAMS;
```

Loopback audio is captured by the **Loopback Thread** into a second ring buffer (`m_loopbackRingBuffer`). Inside the render thread it is **summed** at **0.7× gain** (≈ −3 dB) to prevent peak overload.

**Anti-Feedback Rationale:**

```
Without PID filter:
  SoundXs output → speakers → loopback captures master mix
  → plays again → louder → captures louder → FEEDBACK LOOP

With PID filter (e.g. Spotify only):
  Loopback captures only Spotify's stream
  Mic captures room audio
  → No loop possible
```

---

### Render Thread & Stream Mixing

Every time the render event fires, the render thread does:

```
1. GetCurrentPadding()  → frames already queued in render buffer
2. available = bufferSize - padding
3. IAudioRenderClient::GetBuffer(available)  → get write pointer
4. ring_buffer.read(fBuf, frames × 2)        → copy mic audio
5. if loopback enabled:
     loopback_ring.read(loopBuf, frames × 2)
     for each sample: fBuf[i] += loopBuf[i] × 0.70f
6. dsp.process(fBuf, frames, 2)              → full DSP chain
7. hard clip: clamp each sample to [-0.99, +0.99]
8. peak = peak × 0.9 + framePeak × 0.1       → exponential smoothing
9. IAudioRenderClient::ReleaseBuffer(available, 0)  → commit
```

> **Zero-allocation guarantee:** `loopBuf` is resized lazily once then never again. No `new` or `delete` in the hot path.

---

## DSP Engine Deep Dive

### Biquad Filter Theory

All EQ in SoundXs is based on the **2nd-order IIR biquad filter**. The Z-domain transfer function is:

$$H(z) = \frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}{1 + a_1 z^{-1} + a_2 z^{-2}}$$

In plain terms, the output `y[n]` is:

```
y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2]
               - a1·y[n-1] - a2·y[n-2]
```

Five constant multiplications and four additions per sample — yet this one equation can produce any shelving, peaking, notch, or bandpass response depending on the coefficients.

---

### RBJ Low-Shelf — Bass

**Center frequency:** 80 Hz &nbsp;|&nbsp; **Q:** √2 (Butterworth) &nbsp;|&nbsp; **Range:** ±12 dB

```
A  = 10^(dBgain / 40)
w0 = 2π × f0 / sr
α  = sin(w0)/2 × √2            (shelf slope S = 1)
sA = √A

b0 = A × [(A+1) - (A-1)·cos(w0) + 2·sA·α]
b1 = 2A × [(A-1) - (A+1)·cos(w0)         ]
b2 = A × [(A+1) - (A-1)·cos(w0) - 2·sA·α]
a0 =     [(A+1) + (A-1)·cos(w0) + 2·sA·α]
a1 =-2  × [(A-1) + (A+1)·cos(w0)         ]
a2 =     [(A+1) + (A-1)·cos(w0) - 2·sA·α]
```

All stored as `{b0/a0, b1/a0, b2/a0, a1/a0, a2/a0}`.

---

### RBJ High-Shelf — Treble

**Center frequency:** 10,000 Hz &nbsp;|&nbsp; **Q:** √2 &nbsp;|&nbsp; **Range:** ±12 dB

```
b0 =  A × [(A+1) + (A-1)·cos(w0) + 2·sA·α]
b1 =-2A × [(A-1) + (A+1)·cos(w0)         ]
b2 =  A × [(A+1) + (A-1)·cos(w0) - 2·sA·α]
a0 =      [(A+1) - (A-1)·cos(w0) + 2·sA·α]
a1 = 2  × [(A-1) - (A+1)·cos(w0)         ]
a2 =      [(A+1) - (A-1)·cos(w0) - 2·sA·α]
```

At 48 kHz, the 10 kHz cutoff gives `w0 = 2π × 10000/48000 ≈ 1.309 rad`. This controls the "air" and brightness of a voice.

---

### RBJ Peaking EQ — Presence & Warmth

Unlike shelves, a peaking filter affects only a band around the center frequency:

```
A  = 10^(dBgain / 40)
w0 = 2π × f0 / sr
α  = sin(w0) / (2 × Q)

b0 =  1 + α·A
b1 = -2·cos(w0)
b2 =  1 - α·A
a0 =  1 + α/A
a1 = -2·cos(w0)         ← same as b1, by design
a2 =  1 - α/A
```

| Band | Frequency | Q | Effect |
|---|---|---|---|
| Warmth | 220 Hz | 1.0 | Adds chest resonance and body |
| Presence | 2,800 Hz | 1.0 | Boosts vocal clarity and cut-through |

---

### Transposed Direct Form II

All four biquad stages use **Transposed Direct Form II (TDF-II)** computed in `double` precision to avoid 32-bit rounding error on closely-spaced poles near DC:

```cpp
// One biquad stage (Bass shown):
double y  = bc.b0*x + s.s1;
       s.s1 = bc.b1*x - bc.a1*y + s.s2;
       s.s2 = bc.b2*x - bc.a2*y;
       x = y;   // → next stage
```

The sample flows through `Bass → Treble → Presence → Warmth` in series — four cascaded biquads = 8th-order combined EQ.

**State reset on large jumps:** If gain changes by more than 6 dB in one step, state variables `{s1, s2}` are zeroed to prevent audible clicks:

```cpp
if (std::abs(newDb - oldDb) > 6.0)
    for (auto& s : m_bassState) s.reset();
```

---

### SeqLock — Wait-Free Coefficient Handoff

`SeqLock<T>` lets the UI thread write DSP parameters while the audio thread reads them — **no locks, no queues, no spin-waits** on the hot path:

```cpp
template<typename T>
class SeqLock {
    alignas(64) T m_data{};          // cache-line aligned (no false sharing)
    std::atomic<uint32_t> m_seq{0};  // odd = write in progress

    void store(const T& v) {
        m_seq |= 1;                  // mark dirty
        memcpy(&m_data, &v, sizeof(T));
        m_seq = (m_seq + 1) & ~1u;  // clear dirty, bump generation
    }

    T load() const {
        uint32_t s1, s2;
        T result;
        do {
            s1 = m_seq;
            if (s1 & 1) continue;   // writer active, retry
            memcpy(&result, &m_data, sizeof(T));
            s2 = m_seq;
        } while (s1 != s2);         // changed during read? retry
        return result;
    }
};
```

In practice the retry loop executes **zero times** in the vast majority of calls. The `alignas(64)` ensures the data and sequence counter live on separate cache lines.

---

### Digital Echo / Delay Line

A pre-allocated circular buffer of `sr × 0.5 × 2` floats = **48,000 samples (500 ms stereo)** at 48 kHz.

**Algorithm per sample (stereo interleaved):**

```
output[t]          = input[t] + delayBuf[writePos] × echoMix
delayBuf[writePos] = input[t] + delayBuf[writePos] × feedback

writePos = (writePos + 2) % bufferSize
```

| Parameter | Value |
|---|---|
| Delay time | 500 ms |
| Feedback | 50% fixed (−6 dB per repeat) |
| Wet mix | 0–100% (user slider) |

**Echo decay table:**

| Repeat # | Level vs. original |
|---|---|
| 1st | 50% (−6 dB) |
| 2nd | 25% (−12 dB) |
| 3rd | 12.5% (−18 dB) |
| 4th | 6.25% (−24 dB) |
| nth | (0.5)^n × 100% |

After ~7–8 repeats the echo falls below −42 dB — inaudible, giving a natural tail.

---

### Feed-Forward Peak Compressor

A **4:1 ratio, feed-forward peak-detection** design. For every 4 dB over threshold, only 1 dB passes through.

**Level detection (per frame):**

```
peakDb = 20 × log10(max |sample|)

coef   = (peakDb > envDb) ? attackCoef : releaseCoef
envDb  = coef × envDb + (1 − coef) × peakDb
```

**Time constants at 48,000 Hz:**

| Phase | Time | Coefficient |
|---|---|---|
| Attack | 5 ms | `exp(−1 / (0.005 × 48000))` ≈ 0.9958 |
| Release | 150 ms | `exp(−1 / (0.150 × 48000))` ≈ 0.9999 |

**Gain computation:**

```
if (envDb > threshold):
    gainDb = (threshold − envDb) × (1 − 1/ratio)
           = (threshold − envDb) × 0.75

makeup = −threshold × 0.75 × 0.6     (conservative auto-gain)
linGain = 10^((gainDb + makeup) / 20)
```

---

### Soft Tanh Limiter

The final stage applies a soft saturation to prevent harsh digital clipping:

```cpp
// DSPEngine — soft saturation (musical)
if (sample > 0.95f || sample < -0.95f)
    sample = tanh(sample);

// AudioEngine — hard clip (safety net)
sample = clamp(sample, -0.99f, 0.99f);
```

`tanh(x)` approaches ±1.0 asymptotically, introducing smooth harmonic saturation instead of square-wave buzz:

| Input | tanh(input) |
|---|---|
| 0.95 | 0.7394 |
| 1.00 | 0.7616 |
| 1.50 | 0.9051 |
| 2.00 | 0.9640 |

---

### Full DSP Processing Chain

Every render frame flows through this exact pipeline **in order**:

```
Input PCM Float32
    │
    ▼  1. Echo / Delay
    │   y[n] = x[n] + delay[pos] × echoMix
    │   delay[pos] = x[n] + delay[pos] × 0.5
    │
    ▼  2. RBJ Low-Shelf (Bass, 80 Hz) — double precision TDF-II
    │
    ▼  3. RBJ High-Shelf (Treble, 10 kHz) — double precision TDF-II
    │
    ▼  4. RBJ Peaking EQ (Presence, 2,800 Hz) — Q=1.0
    │
    ▼  5. RBJ Peaking EQ (Warmth, 220 Hz) — Q=1.0
    │
    ▼  6. Master Volume Gain  → x *= 10^(volumeDb / 20)
    │
    ▼  7. Feed-Forward Compressor (4:1, 5 ms attack / 150 ms release)
    │
    ▼  8. Soft Tanh Limiter (|x| > 0.95)
    │
Output PCM Float32 → IAudioRenderClient → Speakers
```

---

## DSP Parameter Reference

| Parameter | Slider Range | Internal Range | Filter Type | Center Freq | Notes |
|---|---|---|---|---|---|
| Bass | −120 to +120 (÷10) | −12 to +12 dB | Low-Shelf | 80 Hz | Q = √2 |
| Treble | −120 to +120 (÷10) | −12 to +12 dB | High-Shelf | 10 kHz | Q = √2 |
| Volume | −200 to +100 (÷10) | −20 to +10 dB | Gain | — | Linear multiply |
| Warmth | −120 to +120 (÷10) | −12 to +12 dB | Peaking EQ | 220 Hz | Q = 1.0 |
| Presence | −120 to +120 (÷10) | −12 to +12 dB | Peaking EQ | 2,800 Hz | Q = 1.0 |
| Compress | −400 to 0 (÷10) | 0 to −40 dB | Threshold | — | 4:1 ratio |
| Echo | 0 to 100 | 0–100% wet | Delay | — | 500 ms, 50% fb |

---

## Performance Targets

| Metric | Target | How |
|---|---|---|
| CPU Usage | < 2% | TDF-II with 5 muls/sample, zero allocations in hot path |
| RAM | < 60 MB | Ring buffers ~2 MB total, Qt UI ~30 MB |
| Audio Latency | ~20 ms | WASAPI shared mode, 200 k hns buffer |
| UI Refresh | ~60 fps | Qt event loop + 60 Hz peak meter timer |
| Startup Time | < 500 ms | COM init + WASAPI enumerate on main thread |
| Thread Priority | Max RT | MMCSS "Pro Audio" + `THREAD_PRIORITY_TIME_CRITICAL` |
| Hot-path Allocs | Zero | Buffers pre-sized; no `new`/`delete` in render loop |

---

## UI & Qt Theme System

SoundXs UI is built in **Qt 6 Widgets** with a fully custom `.qss` skin compiled into the binary via `resources.qrc`. No third-party UI library.

### Widget Hierarchy

```
QMainWindow (SoundXs)
└── QScrollArea
    └── QWidget  (root, QVBoxLayout)
        ├── [Header]
        │   └── QLabel  "SoundXs" / "PRECISION TONE"
        ├── [Input Card]
        │   └── QComboBox  m_inputCombo
        ├── [Output / Preset Card]
        │   ├── QComboBox  m_outputCombo
        │   ├── QComboBox  m_presetCombo
        │   └── QPushButton#loopbackBtn  (checkable)
        ├── [EQ Card]
        │   ├── makeSliderRow("Bass",   ±120)
        │   ├── makeSliderRow("Treble", ±120)
        │   └── makeSliderRow("Volume", −200 → +100)
        ├── [Enhancer Card]
        │   ├── makeSliderRow("Compress", −400 → 0)
        │   ├── makeSliderRow("Presence", ±120)
        │   ├── makeSliderRow("Warmth",   ±120)
        │   └── makeSliderRow("Echo",     0 → 100)
        ├── QProgressBar#peakMeter  (8 px, animated)
        └── QLabel#footer
```

### QSS Theme Highlights

| Widget | Style |
|---|---|
| `QMainWindow` | Background `#0D0D0D` |
| Section cards | `#111111` bg, `1px solid #1E1E1E` border, 8 px radius |
| `QSlider::groove` | 4 px height, `#1A1A1A`, rounded |
| `QSlider::handle` | 14×14 px circle, `#888888`, hover → `#BBBBBB` |
| `QProgressBar#peakMeter` | Gradient `#00BCD4` → `#007A8D` |
| `QPushButton#loopbackBtn` | OFF = `#1A1A1A`; ON = neon `#007AFF` glow border |
| Typography | `"Inter", "Segoe UI", sans-serif` |

### Peak Meter

Driven by a `QTimer` at ~60 Hz. The audio thread writes a smoothed value atomically:

```
newPeak = oldPeak × 0.9 + framePeak × 0.1
```

This is a single-pole IIR lowpass on the peak envelope — identical math to the compressor's envelope follower at a different time constant.

---

## DeviceManager — Hot-Plug Detection

`DeviceManager` implements `IMMNotificationClient` — the COM callback interface Windows calls when devices are plugged in or removed:

```cpp
HRESULT OnDeviceAdded(LPCWSTR pwstrDeviceId)          override;
HRESULT OnDeviceRemoved(LPCWSTR pwstrDeviceId)         override;
HRESULT OnDeviceStateChanged(LPCWSTR id, DWORD state)  override;
HRESULT OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR id) override;
```

When any fires:
1. `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` marshals onto the UI thread
2. `MainWindow::onDevicesChanged()` re-enumerates and repopulates both combo boxes
3. If the current device was removed, the engine falls back to the default endpoint

Device IDs are Windows **Endpoint ID strings** like:  
`{0.0.0.00000000}.{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}`

---

## SettingsManager — Persistence

Settings are stored via `QSettings` (Org: `"Inbora"`, App: `"SoundXs"`) → Windows Registry:

```
HKEY_CURRENT_USER\Software\Inbora\SoundXs
```

The `AppSettings` struct:

```cpp
struct AppSettings {
    QString inputDeviceId;
    QString outputDeviceId;
    double  bassDb     = 0.0;
    double  trebleDb   = 0.0;
    double  volumeDb   = 0.0;
    double  echoAmount = 0.0;
    QString preset     = "Flat";
};
```

**On startup:** `m_loading = true` → sliders set → DSP updated → `m_loading = false`  
**On every slider move:** `saveSettings()` → `QSettings::setValue()` → `sync()` (immediate flush)

---

## Preset System Internals

| Preset | Bass | Treble | Volume | Effect |
|---|---|---|---|---|
| **Flat** | 0 dB | 0 dB | 0 dB | Unity gain — clean slate |
| **Bass Boost** | +8 dB | −2 dB | 0 dB | Sub-heavy, music monitoring |
| **Treble Boost** | −2 dB | +8 dB | 0 dB | Bright, airy, detail-forward |

Because `setValue()` fires each slider's signal, the DSP is updated **live during preset switching** — you hear EQ change sample-accurately before the preset fully applies.

---

## Memory Layout Summary

| Object | Size | Notes |
|---|---|---|
| Mic Ring Buffer | 512 KB | 2^17 × float32 |
| Loopback Ring Buffer | 512 KB | Same size |
| Echo Delay Buffer | 192 KB | 500 ms stereo @ 48 kHz |
| DspParams (SeqLock) | 64 B | Cache-line aligned |
| Biquad Coefficients | 160 B | 4 filters × 5 doubles |
| Biquad State | 512 B | 4 filters × 2 ch × 2 doubles |
| WASAPI COM objects | ~3 KB | IMMDevice, IAudioClient, etc. |
| Qt Widgets (UI) | ~25 MB | Fonts, pixmaps, palettes |
| **Total** | **< 40 MB** | Well under 60 MB target |

---

## Project Structure

```
SoundXs/
│
├── CMakeLists.txt                  # Build definition, windeployqt post-build
│
├── include/
│   ├── AudioEngine.h               # WASAPI engine, ring buffer, 3-thread model
│   ├── DSPEngine.h                 # RBJ biquads, SeqLock, DspParams, delay buffer
│   ├── DeviceManager.h             # IMMDeviceEnumerator + IMMNotificationClient
│   ├── SettingsManager.h           # AppSettings struct + QSettings CRUD
│   └── MainWindow.h                # Qt 6 UI: sliders, labels, peak meter
│
├── src/
│   ├── main.cpp                    # QApplication, icon, show()
│   ├── AudioEngine.cpp             # WASAPI lifecycle, ring buffer, thread funcs
│   ├── DSPEngine.cpp               # RBJ math, process() hot loop, compressor, echo
│   ├── DeviceManager.cpp           # Endpoint enumeration, COM notification sink
│   ├── SettingsManager.cpp         # QSettings load/save for all DSP params
│   └── MainWindow.cpp              # Widget construction, slot handlers, presets
│
├── resources/
│   ├── resources.qrc               # Qt resource bundle
│   ├── style.qss                   # Full dark QSS theme
│   ├── logo.png                    # SoundXs logo (Qt resource — window icon)
│   └── app.ico                     # Multi-resolution icon (EXE file icon)
│
├── Logo/
│   └── Sound Xs.png                # Original logo asset
│
└── build.bat                       # One-click build script for Windows
```

---

## Building from Source

### Prerequisites

| Tool | Version | Notes |
|---|---|---|
| Windows | 10 or 11 | Per-process loopback requires Win10 2004+ |
| Qt | 6.6+ | MinGW 64-bit kit via Qt Online Installer |
| CMake | 3.25+ | |
| Ninja | Latest | Bundled with Qt Tools |
| MinGW | 13.x | Bundled with Qt (`mingw1310_64`) |

### Option A — One-Click Build Script

```bat
build.bat
```

### Option B — Manual CMake

```powershell
# Set paths (adjust to your Qt install)
$env:PATH = "D:\QTx\Tools\mingw1310_64\bin;D:\QTx\Tools\Ninja;D:\QTx\6.10.2\mingw_64\bin;" + $env:PATH

# Configure
cmake -B build -G "Ninja" `
      -DCMAKE_BUILD_TYPE=Release `
      -DQt6_DIR="D:\QTx\6.10.2\mingw_64\lib\cmake\Qt6"

# Build
cmake --build build --config Release
```

### Run

```
build\SoundXs.exe
```

`windeployqt` runs automatically as a post-build step — all Qt DLLs are placed next to the executable.

---

## FAQ

**Q: Why WASAPI and not ASIO?**  
A: WASAPI ships with every copy of Windows 10/11 — zero driver installation. Event-driven shared mode achieves near-ASIO latency for our ~20 ms target.

**Q: Why not Windows APO (Audio Processing Object)?**  
A: APOs require driver signing and WHQL certification. SoundXs is a clean userland application — close it any time, zero system modification.

**Q: Why 48 kHz and not 44.1 kHz?**  
A: Windows WASAPI shared mode internally mixes at 48 kHz on essentially all modern hardware. Requesting 48 kHz avoids a redundant resample hop and maximises filter headroom near Nyquist (24 kHz).

**Q: Why `double` precision for biquad math?**  
A: 32-bit `float` has only 7 significant digits. The 80 Hz low-shelf at 48 kHz has poles very close to `z = 1.0` (near DC). Subtraction of near-equal values causes catastrophic cancellation in `float`. `double` gives 15 digits — the filter stays stable across all practical gain settings.

**Q: Why does desktop capture require selecting a PID?**  
A: Without PID filtering, the loopback captures the entire master mix — including SoundXs's own output, creating a feedback loop. Targeting a specific app (e.g. Spotify) captures only that stream, breaking the loop.

**Q: Can I use SoundXs as a virtual mic for Discord/Zoom?**  
A: Not directly yet. Set a virtual audio cable (VB-Cable, Voicemeeter) as the output device to route processed audio to any app as a microphone source.

**Q: What is the total end-to-end latency?**  
A: WASAPI capture (~10 ms) + ring buffer queuing (~0–10 ms) + WASAPI render (~10 ms) = **~20–30 ms**. DSP itself adds **zero samples** of latency — samples are processed in-place as they pass through.

---

## Contributing

Contributions are welcome! Ideas for future features:

- [ ] Noise Gate — silence background noise below a threshold
- [ ] Stereo Widener — Haas effect / mid-side processing
- [ ] Spectral Visualizer — real-time FFT frequency display
- [ ] VST Plugin Hosting — load third-party plugins in the DSP chain
- [ ] Multi-preset Bank — save/load named presets to JSON

**Guidelines:**
1. Keep DSP math in `DSPEngine.cpp` — UI in `MainWindow.cpp`
2. No allocations inside `DSPEngine::process()` or any `*ThreadFunc()`
3. All new parameters must go through `SeqLock<DspParams>` — never raw shared state

---

## License

```
MIT License

Copyright (c) 2026 Inbora Studio / drchamyoung

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

<div align="center">

**Built with obsession by [Inbora Studio](https://github.com/InboraStudio)**

*drchamyoung — "Every sample. Every microsecond. Every dB."*

<br/>

## Table of Contents
---
- [What Is SoundXs?](#what-is-soundxs)
- [Feature Matrix](#feature-matrix)
- [System Architecture](#system-architecture)
- [Thread Model](#thread-model)
- [Data Flow Diagram](#data-flow-diagram)
- [Audio Engine Deep Dive](#audio-engine-deep-dive)
- [WASAPI Integration](#wasapi-integration)
- [Lock-Free SPSC Ring Buffer](#lock-free-spsc-ring-buffer)
- [Desktop Loopback Capture](#desktop-loopback-capture)
- [Render Thread & Stream Mixing](#render-thread--stream-mixing)
- [DSP Engine Deep Dive](#dsp-engine-deep-dive)
- [Biquad Filter Theory](#biquad-filter-theory)
- [RBJ Low-Shelf — Bass](#rbj-low-shelf--bass)
- [RBJ High-Shelf — Treble](#rbj-high-shelf--treble)
- [RBJ Peaking EQ — Presence & Warmth](#rbj-peaking-eq--presence--warmth)
- [Transposed Direct Form II](#transposed-direct-form-ii)
- [SeqLock — Wait-Free Coefficient Handoff](#seqlock--wait-free-coefficient-handoff)
- [Digital Echo / Delay Line](#digital-echo--delay-line)
- [Feed-Forward Peak Compressor](#feed-forward-peak-compressor)
- [Soft Tanh Limiter](#soft-tanh-limiter)
- [Full DSP Processing Chain](#full-dsp-processing-chain)
- [DSP Parameter Reference](#dsp-parameter-reference)
- [Performance Targets](#performance-targets)
- [UI & Qt Theme System](#ui--qt-theme-system)
- [DeviceManager — Hot-Plug Detection](#devicemanager--hot-plug-detection)
- [SettingsManager — Persistence](#settingsmanager--persistence)
- [Preset System Internals](#preset-system-internals)
- [Memory Layout Summary](#memory-layout-summary)
- [Project Structure](#project-structure)
- [Building from Source](#building-from-source)
- [FAQ](#faq)
- [Contributing](#contributing)
- [License](#license)
---

⭐ If SoundXs helped your streams, recordings, or calls — drop a star. It means the world.

</div>

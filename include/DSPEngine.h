#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
struct BiquadCoeffs {
  double b0 = 1.0, b1 = 0.0, b2 = 0.0;
  double a1 = 0.0, a2 = 0.0; 
};
struct BiquadState {
  double s1 = 0.0, s2 = 0.0;
  void reset() noexcept { s1 = s2 = 0.0; }
};
template <typename T> class SeqLock {
public:
  void store(const T &v) noexcept {
    uint32_t s = m_seq.load(std::memory_order_relaxed);
    m_seq.store(s | 1u, std::memory_order_release);
    std::memcpy(&m_data, &v, sizeof(T));
    m_seq.store((s + 2u) & ~1u, std::memory_order_release);
  }
  T load() const noexcept {
    T result;
    uint32_t s1, s2;
    do {
      s1 = m_seq.load(std::memory_order_acquire);
      if (s1 & 1u) {
        continue;
      }
      std::memcpy(&result, &m_data, sizeof(T));
      s2 = m_seq.load(std::memory_order_acquire);
    } while (s1 != s2);
    return result;
  }

private:
  alignas(64) T m_data{};
  std::atomic<uint32_t> m_seq{0};
};
struct DspParams {
  double bassDb = 0.0;
  double trebleDb = 0.0;
  double volumeDb = 0.0;
  double presenceDb = 0.0;   
  double warmthDb = 0.0;     
  double compThreshDb = 0.0; 
  double echoAmount = 0.0;   
};
class DSPEngine {
public:
  void setSampleRate(double sr);
  void setBass(double dB);
  void setTreble(double dB);
  void setVolume(double dB);
  void setPresence(double dB);         
  void setWarmth(double dB);           
  void setCompressor(double threshDb); 
  void setEcho(double amount);         
  void process(float *buf, int frames, int channels) noexcept;

  double sampleRate() const noexcept { return m_sampleRate; }

private:
  void updateCoeffsIfNeeded() noexcept;

  static BiquadCoeffs makeLowShelf(double sr, double freq,
                                   double gainDb) noexcept;
  static BiquadCoeffs makeHighShelf(double sr, double freq,
                                    double gainDb) noexcept;
  static BiquadCoeffs makePeakingEQ(double sr, double freq, double gainDb,
                                    double Q) noexcept;

  double m_sampleRate = 48000.0;

  SeqLock<DspParams> m_params;
  DspParams m_cachedParams; 

  BiquadCoeffs m_bassCoeffs;
  BiquadCoeffs m_trebleCoeffs;
  BiquadCoeffs m_presenceCoeffs;
  BiquadCoeffs m_warmthCoeffs;
  float m_volumeLinear = 1.0f;
  float m_limiterGain = 1.0f;

  static constexpr int MAX_CH = 8;
  BiquadState m_bassState[MAX_CH];
  BiquadState m_trebleState[MAX_CH];
  BiquadState m_presenceState[MAX_CH];
  BiquadState m_warmthState[MAX_CH];
  float m_compEnvDb = -120.0f; 
  float m_compAttCoef = 0.0f;
  float m_compRelCoef = 0.0f;
  static constexpr size_t DELAY_SIZE = 48000; 
  std::vector<float> m_delayBuffer;
  size_t m_delayWritePos = 0;
};

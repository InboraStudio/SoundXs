#include "DSPEngine.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
BiquadCoeffs DSPEngine::makeLowShelf(double sr, double f0,
                                     double dBgain) noexcept {
  double A = std::pow(10.0, dBgain / 40.0);
  double w0 = 2.0 * M_PI * f0 / sr;
  double cw = std::cos(w0);
  double sw = std::sin(w0);
  double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);
  double sA = std::sqrt(A);
  alpha = sw / 2.0 * std::sqrt(2.0);

  double b0 = A * ((A + 1.0) - (A - 1.0) * cw + 2.0 * sA * alpha);
  double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
  double b2 = A * ((A + 1.0) - (A - 1.0) * cw - 2.0 * sA * alpha);
  double a0 = ((A + 1.0) + (A - 1.0) * cw + 2.0 * sA * alpha);
  double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
  double a2 = ((A + 1.0) + (A - 1.0) * cw - 2.0 * sA * alpha);

  return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
BiquadCoeffs DSPEngine::makePeakingEQ(double sr, double f0, double dBgain,
                                      double Q) noexcept {
  double A = std::pow(10.0, dBgain / 40.0);
  double w0 = 2.0 * M_PI * f0 / sr;
  double alpha = std::sin(w0) / (2.0 * Q);

  double b0 = 1.0 + alpha * A;
  double b1 = -2.0 * std::cos(w0);
  double b2 = 1.0 - alpha * A;
  double a0 = 1.0 + alpha / A;
  double a1 = -2.0 * std::cos(w0);
  double a2 = 1.0 - alpha / A;
  return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
BiquadCoeffs DSPEngine::makeHighShelf(double sr, double f0,
                                      double dBgain) noexcept {
  double A = std::pow(10.0, dBgain / 40.0);
  double w0 = 2.0 * M_PI * f0 / sr;
  double cw = std::cos(w0);
  double sw = std::sin(w0);
  double sA = std::sqrt(A);
  double alpha = sw / 2.0 * std::sqrt(2.0); // S = 1

  double b0 = A * ((A + 1.0) + (A - 1.0) * cw + 2.0 * sA * alpha);
  double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
  double b2 = A * ((A + 1.0) + (A - 1.0) * cw - 2.0 * sA * alpha);
  double a0 = ((A + 1.0) - (A - 1.0) * cw + 2.0 * sA * alpha);
  double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
  double a2 = ((A + 1.0) - (A - 1.0) * cw - 2.0 * sA * alpha);

  return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
void DSPEngine::setSampleRate(double sr) {
  m_sampleRate = sr;
  
  m_compAttCoef =
      std::exp(-1.0f / (0.005f * static_cast<float>(sr))); 
  m_compRelCoef =
      std::exp(-1.0f / (0.150f * static_cast<float>(sr))); 

  size_t newSize = static_cast<size_t>(sr * 0.5 * 2); 
  if (m_delayBuffer.size() != newSize) {
    m_delayBuffer.assign(newSize, 0.0f);
    m_delayWritePos = 0;
  }

  m_cachedParams.bassDb = m_cachedParams.trebleDb = 1e9;
  m_cachedParams.presenceDb = m_cachedParams.warmthDb = 1e9;
  m_cachedParams.compThreshDb = 1e9;
  m_cachedParams.echoAmount = 1e9;
}

void DSPEngine::setBass(double dB) {
  DspParams p = m_params.load();
  p.bassDb = dB;
  m_params.store(p);
}

void DSPEngine::setTreble(double dB) {
  DspParams p = m_params.load();
  p.trebleDb = dB;
  m_params.store(p);
}

void DSPEngine::setVolume(double dB) {
  DspParams p = m_params.load();
  p.volumeDb = dB;
  m_params.store(p);
}
void DSPEngine::setPresence(double dB) {
  DspParams p = m_params.load();
  p.presenceDb = dB;
  m_params.store(p);
}

void DSPEngine::setWarmth(double dB) {
  DspParams p = m_params.load();
  p.warmthDb = dB;
  m_params.store(p);
}

void DSPEngine::setCompressor(double threshDb) {
  DspParams p = m_params.load();
  p.compThreshDb = threshDb;
  m_params.store(p);
}

void DSPEngine::setEcho(double amount) {
  DspParams p = m_params.load();
  p.echoAmount = amount / 10.0; // scale 0-10 to 0-1.0
  m_params.store(p);
}

void DSPEngine::updateCoeffsIfNeeded() noexcept {
  DspParams cur = m_params.load();
  bool bassChanged = (cur.bassDb != m_cachedParams.bassDb);
  bool trebleChanged = (cur.trebleDb != m_cachedParams.trebleDb);
  bool volChanged = (cur.volumeDb != m_cachedParams.volumeDb);
  bool presenceChanged = (cur.presenceDb != m_cachedParams.presenceDb);
  bool warmthChanged = (cur.warmthDb != m_cachedParams.warmthDb);
  bool compChanged = (cur.compThreshDb != m_cachedParams.compThreshDb);

  if (bassChanged) {
    m_bassCoeffs = makeLowShelf(m_sampleRate, 80.0, cur.bassDb);
    if (std::abs(cur.bassDb - m_cachedParams.bassDb) > 6.0)
      for (auto &s : m_bassState)
        s.reset();
  }
  if (trebleChanged) {
    m_trebleCoeffs = makeHighShelf(m_sampleRate, 10000.0, cur.trebleDb);
    if (std::abs(cur.trebleDb - m_cachedParams.trebleDb) > 6.0)
      for (auto &s : m_trebleState)
        s.reset();
  }
  if (presenceChanged) {
    m_presenceCoeffs = makePeakingEQ(m_sampleRate, 2800.0, cur.presenceDb, 1.0);
    if (std::abs(cur.presenceDb - m_cachedParams.presenceDb) > 6.0)
      for (auto &s : m_presenceState)
        s.reset();
  }
  if (warmthChanged) {
    m_warmthCoeffs = makePeakingEQ(m_sampleRate, 220.0, cur.warmthDb, 1.0);
    if (std::abs(cur.warmthDb - m_cachedParams.warmthDb) > 6.0)
      for (auto &s : m_warmthState)
        s.reset();
  }
  if (volChanged) {
    m_volumeLinear = static_cast<float>(std::pow(10.0, cur.volumeDb / 20.0));
  }
  if (compChanged) {
    m_compEnvDb = -120.0f; 
  }

  m_cachedParams = cur;
}

void DSPEngine::process(float *buf, int frames, int channels) noexcept {
  updateCoeffsIfNeeded();

  const int ch = std::min(channels, MAX_CH);
  const BiquadCoeffs &bc = m_bassCoeffs;
  const BiquadCoeffs &tc = m_trebleCoeffs;
  const BiquadCoeffs &pc = m_presenceCoeffs;
  const BiquadCoeffs &wc = m_warmthCoeffs;

  const float thresh = static_cast<float>(m_cachedParams.compThreshDb);
  const float ratio = 4.0f; // fixed 4:1
  const float makeup =
      (thresh < 0.0f) ? (-thresh * (1.0f - 1.0f / ratio) * 0.6f) : 0.0f;
  const bool compOn = (thresh < -0.5f);

  const float echoMix = static_cast<float>(m_cachedParams.echoAmount);
  const float echoFeedback = 0.5f; // fixed 50% feedback for "nice" repeats
  const bool echoOn = (echoMix > 0.01f);

  for (int f = 0; f < frames; ++f) {
  
    if (echoOn && !m_delayBuffer.empty()) {
      for (int c = 0; c < ch && c < 2; ++c) {
        int idx = f * channels + c;
        float inputSample = buf[idx];

        float delayedSample = m_delayBuffer[m_delayWritePos + c];

        buf[idx] += delayedSample * echoMix;

        m_delayBuffer[m_delayWritePos + c] =
            inputSample + delayedSample * echoFeedback;
      }
      m_delayWritePos += 2; // Stereo delay
      if (m_delayWritePos >= m_delayBuffer.size())
        m_delayWritePos = 0;
    }

    float peakAbs = 0.0f;
    for (int c = 0; c < ch; ++c) {
      int idx = f * channels + c;
      double x = buf[idx];

      {
        auto &s = m_bassState[c];
        double y = bc.b0 * x + s.s1;
        s.s1 = bc.b1 * x - bc.a1 * y + s.s2;
        s.s2 = bc.b2 * x - bc.a2 * y;
        x = y;
      }
    
      {
        auto &s = m_trebleState[c];
        double y = tc.b0 * x + s.s1;
        s.s1 = tc.b1 * x - tc.a1 * y + s.s2;
        s.s2 = tc.b2 * x - tc.a2 * y;
        x = y;
      }
     
      {
        auto &s = m_presenceState[c];
        double y = pc.b0 * x + s.s1;
        s.s1 = pc.b1 * x - pc.a1 * y + s.s2;
        s.s2 = pc.b2 * x - pc.a2 * y;
        x = y;
      }
   
      {
        auto &s = m_warmthState[c];
        double y = wc.b0 * x + s.s1;
        s.s1 = wc.b1 * x - wc.a1 * y + s.s2;
        s.s2 = wc.b2 * x - wc.a2 * y;
        x = y;
      }

     
      x *= m_volumeLinear;

      buf[idx] = static_cast<float>(x);
      peakAbs = std::max(peakAbs, std::abs(static_cast<float>(x)));
    }

    if (compOn) {
      float peakDb = (peakAbs > 1e-9f) ? 20.0f * std::log10(peakAbs) : -120.0f;
      float coef = (peakDb > m_compEnvDb) ? m_compAttCoef : m_compRelCoef;
      m_compEnvDb = coef * m_compEnvDb + (1.0f - coef) * peakDb;

      float gainDb = 0.0f;
      if (m_compEnvDb > thresh)
        gainDb = (thresh - m_compEnvDb) * (1.0f - 1.0f / ratio);
      float linGain = std::pow(10.0f, (gainDb + makeup) / 20.0f);

      for (int c = 0; c < ch; ++c)
        buf[f * channels + c] *= linGain;
    }

    for (int c = 0; c < ch; ++c) {
      float &s = buf[f * channels + c];
      if (s > 0.95f || s < -0.95f)
        s = static_cast<float>(std::tanh(s));
    }
  }
}

#pragma once

#include "ISender.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace plugin_dsp {

// Drives a peak-style level meter. For each channel, tracks a peak envelope with instant
// attack and exponential decay, sampled at full audio rate, and sends it to a UI control
// via ISender's queue whenever it changes.
template <int MAXNC = 2, int QUEUE_SIZE = 64> class PeakEnvelopeSender : public iplug::ISender<MAXNC, QUEUE_SIZE, float> {
public:
  // fallTimeMs is roughly how long the envelope takes to fall from full scale to inaudible
  // once the signal stops producing new peaks.
  void Reset(double sampleRate, double fallTimeMs = 200.0) {
    const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double samples = std::max(1.0, fallTimeMs * 0.001 * safeSampleRate);
    mDecayPerSample = static_cast<float>(std::pow(kFallTargetRatio, 1.0 / samples));
    mEnvelope.fill(0.0f);
    mLastPushed.fill(-1.0f); // Impossible level, so the first block after a reset always pushes.
  }

  void ProcessBlock(iplug::sample** inputs, int nFrames, int ctrlTag, int nChans = MAXNC, int chanOffset = 0) {
    iplug::ISenderData<MAXNC, float> data{ctrlTag, nChans, chanOffset};
    bool changed = false;

    for (int c = chanOffset; c < chanOffset + nChans; ++c) {
      float envelope = mEnvelope[c];

      // Report the highest level reached at any point in the block rather than the envelope's
      // end-of-block value, so a brief peak early in a large block cannot decay away unseen.
      float blockPeak = envelope;

      for (int s = 0; s < nFrames; ++s) {
        const float absVal = std::fabs(static_cast<float>(inputs[c][s]));
        envelope = (absVal > envelope) ? absVal : (envelope * mDecayPerSample);
        if (absVal > blockPeak) blockPeak = absVal;
      }

      // Snap to exact silence below an inaudible floor rather than letting the exponential
      // decay approach zero forever, which would eventually multiply denormal floats every
      // sample (slow on many CPUs) and never let the changed check settle.
      if (envelope < kFloorLevel) envelope = 0.0f;
      if (blockPeak < kFloorLevel) blockPeak = 0.0f;

      mEnvelope[c] = envelope;
      if (blockPeak != mLastPushed[c]) changed = true;
      data.vals[c] = blockPeak;
      mLastPushed[c] = blockPeak;
    }

    if (changed) this->PushData(data);
  }

private:
  static constexpr double kFallTargetRatio = 1.0e-3; // Envelope falls to ~-60 dB of its peak after the configured fall time.
  static constexpr float kFloorLevel = 1.0e-5f;       // Well below the meter's visible range (~-73.5 dB, ~2.1e-4 linear).

  float mDecayPerSample = 0.999f;
  std::array<float, MAXNC> mEnvelope{};
  std::array<float, MAXNC> mLastPushed{};
};

} // namespace plugin_dsp

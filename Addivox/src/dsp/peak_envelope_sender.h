#pragma once

#include "ISender.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace plugin_dsp {

// Sends block peaks to the UI, with instant attack and exponential decay between peaks.
template <int MAXNC = 2, int QUEUE_SIZE = 64> class PeakEnvelopeSender : public iplug::ISender<MAXNC, QUEUE_SIZE, float> {
public:
  // fallTimeMs is the time to decay by 60 dB without new peaks.
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

      // Include the starting envelope so peaks remain visible across block boundaries.
      float blockPeak = envelope;

      for (int s = 0; s < nFrames; ++s) {
        const float magnitude = std::fabs(static_cast<float>(inputs[c][s]));
        envelope = (magnitude > envelope) ? magnitude : (envelope * mDecayPerSample);
        blockPeak = std::max(blockPeak, magnitude);
      }

      // Stop sending updates at silence and avoid denormal decay.
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

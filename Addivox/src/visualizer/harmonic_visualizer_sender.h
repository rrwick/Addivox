#pragma once

#include "ISender.h"

#include <algorithm>
#include <cmath>
#include <utility>

template <typename TFrame, int QUEUE_SIZE = 128> class HarmonicVisualizerSender : public iplug::ISender<1, QUEUE_SIZE, TFrame> {
public:
  void Reset(double sampleRate, double targetFPS = 60.0) {
    const double safeSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    const double safeFPS = (targetFPS > 0.0) ? targetFPS : 60.0;
    mSamplesPerUpdate = std::max(1, static_cast<int>(std::lround(safeSampleRate / safeFPS)));
    mSamplesUntilNextUpdate = 0;
  }

  template <typename FillFrameFunc> void ProcessBlock(int nFrames, int ctrlTag, FillFrameFunc&& fillFrame) {
    int framesRemaining = nFrames;

    while (framesRemaining > 0) {
      if (mSamplesUntilNextUpdate <= 0) {
        iplug::ISenderData<1, TFrame> data{ctrlTag, 1, 0};
        fillFrame(data.vals[0]);
        this->PushData(data);
        mSamplesUntilNextUpdate = mSamplesPerUpdate;
      }

      const int advance = std::min(framesRemaining, mSamplesUntilNextUpdate);
      mSamplesUntilNextUpdate -= advance;
      framesRemaining -= advance;
    }
  }

  void ClearQueuedData() {
    iplug::ISenderData<1, TFrame> discarded;
    while (this->mQueue.Pop(discarded)) {
    }
  }

  void PushFrame(int ctrlTag, const TFrame& frame) {
    iplug::ISenderData<1, TFrame> data{ctrlTag, 1, 0};
    data.vals[0] = frame;
    this->PushData(data);
  }

private:
  int mSamplesPerUpdate{735}; // 44.1 kHz / 60 Hz
  int mSamplesUntilNextUpdate{0};
};

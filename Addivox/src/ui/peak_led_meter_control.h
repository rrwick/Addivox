#pragma once

#include "IControls.h"

#include <algorithm>
#include <cmath>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

class PeakLEDMeterControl final : public IVLEDMeterControl<2> {
public:
  using IVLEDMeterControl<2>::IVLEDMeterControl;

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override {
    if (IsDisabled() || msgTag != ISender<>::kUpdateMessage) return;

    IByteStream stream(pData, dataSize);
    ISenderData<2, float> data;
    stream.Get(&data, 0);

    const double lowRangeDb = static_cast<double>(mLowRangeDB);
    const double highRangeDb = static_cast<double>(mHighRangeDB);
    const double rangeDb = std::fabs(highRangeDb - lowRangeDb);
    if (rangeDb <= 0.0) return;

    for (int channel = data.chanOffset; channel < (data.chanOffset + data.nChans); ++channel) {
      const double peak = static_cast<double>(data.vals[channel]);
      double meterValue = 0.0;
      if (peak > 0.0) {
        const double peakDb = AmpToDB(peak);
        meterValue = (peakDb - lowRangeDb) / rangeDb;
        if (peak >= 1.0) meterValue += 1.0e-6; // Ensure exactly 0 dBFS lights the first red LED segment.
      }

      SetValue(Clip(meterValue, 0.0, 1.0), channel);
    }

    SetDirty(false);
  }
};
} // namespace plugin_ui

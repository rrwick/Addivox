#pragma once

#include "IControl.h"
#include "IPlugStructs.h"
#include "ISender.h"
#include "colour.h"
#include "harmonic_visualiser_frame.h"

#include <algorithm>
#include <cmath>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

namespace harmonic_visualiser_level
{
inline float MapPseudoLog(float level)
{
  // Isolated intensity mapping so this can be swapped easily.
  constexpr float kShape = 32.f;

  const float safeLevel = std::max(0.f, level);
  const float numerator = std::log1pf(kShape * safeLevel);
  const float denominator = std::log1pf(kShape);

  if(denominator <= 0.f)
    return 0.f;

  return std::clamp(numerator / denominator, 0.f, 1.f);
}
} // namespace harmonic_visualiser_level

class HarmonicVisualiserControl : public IControl
{
public:
  explicit HarmonicVisualiserControl(const IRECT& bounds)
  : IControl(bounds)
  {
  }

  void Draw(IGraphics& g) override
  {
    constexpr float kCornerRadius = 7.f;
    constexpr float kBarWidthPixels = 4.f;
    constexpr float kHalfBarWidth = kBarWidthPixels * 0.5f;

    g.FillRoundRect(plugin_ui::colour::visualizer::kBackground, mRECT, kCornerRadius, &mBlend);
    g.DrawRoundRect(plugin_ui::colour::visualizer::kFrame, mRECT, kCornerRadius, &mBlend, 1.f);

    const IRECT plot = mRECT.GetPadded(-2.f);
    const float centerY = plot.MH();
    const float channelHeight = (plot.H() * 0.5f) - 1.f;

    g.DrawLine(plugin_ui::colour::visualizer::kCenterLine, plot.L, centerY, plot.R, centerY, &mBlend, 1.f);

    for(const auto& osc : mFrame.harmonics)
    {
      if(osc.frequencyHz < kMinFrequencyHz || osc.frequencyHz > kMaxFrequencyHz || osc.level <= 0.f)
        continue;

      const float x = FrequencyToX(osc.frequencyHz, plot);
      const float mappedLevel = harmonic_visualiser_level::MapPseudoLog(osc.level);

      const float leftHeight = mappedLevel * std::clamp(osc.panLeftGain, 0.f, 1.f) * channelHeight;
      const float rightHeight = mappedLevel * std::clamp(osc.panRightGain, 0.f, 1.f) * channelHeight;

      if(leftHeight > 0.f)
      {
        g.FillRect(plugin_ui::colour::visualizer::kLegacyBar, IRECT(x - kHalfBarWidth, centerY - leftHeight, x + kHalfBarWidth, centerY), &mBlend);
      }

      if(rightHeight > 0.f)
      {
        g.FillRect(plugin_ui::colour::visualizer::kLegacyBar, IRECT(x - kHalfBarWidth, centerY, x + kHalfBarWidth, centerY + rightHeight), &mBlend);
      }
    }
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if(IsDisabled() || msgTag != ISender<>::kUpdateMessage)
      return;

    IByteStream stream(pData, dataSize);
    ISenderData<1, HarmonicVisualiserFrame> data;
    int pos = 0;
    pos = stream.Get(&data, pos);

    if(pos >= 0)
    {
      mFrame = data.vals[0];
      SetDirty(false);
    }
  }

private:
  static float FrequencyToX(float frequencyHz, const IRECT& bounds)
  {
    const float clampedFrequency = std::clamp(frequencyHz, kMinFrequencyHz, kMaxFrequencyHz);

    const float logMin = std::log(kMinFrequencyHz);
    const float logMax = std::log(kMaxFrequencyHz);
    const float xNorm = (std::log(clampedFrequency) - logMin) / (logMax - logMin);
    return bounds.L + (xNorm * bounds.W());
  }

  static constexpr float kMinFrequencyHz = 20.f;
  static constexpr float kMaxFrequencyHz = 20000.f;
  HarmonicVisualiserFrame mFrame{};
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE

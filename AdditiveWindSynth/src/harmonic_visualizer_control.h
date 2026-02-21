#pragma once

#include "IControl.h"
#include "IPlugStructs.h"
#include "ISender.h"
#include "harmonic_visualizer_frame.h"

#include <algorithm>
#include <array>
#include <cmath>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

namespace harmonic_visualizer_level
{
inline float MapPseudoLog(float level)
{
  // Isolated intensity mapping so this can be swapped easily.
  constexpr float kShape = 64.f;

  const float safeLevel = std::max(0.f, level);
  const float numerator = std::log1pf(kShape * safeLevel);
  const float denominator = std::log1pf(kShape);

  if(denominator <= 0.f)
    return 0.f;

  return std::clamp(numerator / denominator, 0.f, 1.f);
}
} // namespace harmonic_visualizer_level

class HarmonicVisualizerControl : public IControl
{
public:
  explicit HarmonicVisualizerControl(const IRECT& bounds)
  : IControl(bounds)
  {
  }

  void Draw(IGraphics& g) override
  {
    const IColor kBackgroundColor{255, 8, 10, 12};
    const IColor kFrameColor{255, 42, 48, 56};
    const IColor kCenterLineColor{255, 74, 82, 92};
    const IColor kGridLineColorMinor{75, 70, 78, 88};
    const IColor kGridLineColorMajor{130, 92, 102, 118};
    const IColor kLabelColor{220, 172, 186, 206};
    const IColor kHarmonicColorStart{255, 0, 245, 255};  // bright cyan
    const IColor kHarmonicColorEnd{255, 255, 0, 190};    // hot pink
    const IColor kWhite{255, 255, 255, 255};
    constexpr float kCornerRadius = 7.f;
    constexpr float kLabelAreaHeight = 18.f;
    constexpr float kGlowOuterThickness = 8.f;
    constexpr float kGlowInnerThickness = 4.f;
    constexpr float kCoreThickness = 1.2f;

    g.FillRoundRect(kBackgroundColor, mRECT, kCornerRadius, &mBlend);
    g.DrawRoundRect(kFrameColor, mRECT, kCornerRadius, &mBlend, 1.f);

    const IRECT plot = mRECT.GetPadded(-2.f);
    const IRECT barPlot{plot.L, plot.T, plot.R, plot.B - kLabelAreaHeight};
    const IRECT labelArea{plot.L, barPlot.B, plot.R, plot.B};
    const IText labelText{11.f, kLabelColor, "Roboto-Regular", EAlign::Center, EVAlign::Bottom};
    const float blendWeight = BlendWeight(&mBlend);
    const IBlend glowOuterBlend{EBlend::Add, 0.34f * blendWeight};
    const IBlend glowInnerBlend{EBlend::Add, 0.80f * blendWeight};
    const IBlend coreBlend{EBlend::SrcOver, blendWeight};

    for(const float frequencyHz : kGridFrequenciesHz)
    {
      const float x = FrequencyToX(frequencyHz, barPlot);
      const bool major = IsLabelFrequency(frequencyHz);
      g.DrawLine(
        major ? kGridLineColorMajor : kGridLineColorMinor,
        x,
        barPlot.T,
        x,
        barPlot.B,
        &mBlend,
        major ? 1.1f : 1.f);
    }

    const float centerY = barPlot.MH();
    const float channelHeight = (barPlot.H() * 0.5f) - 1.f;
    constexpr float kYAxisBoost = 1.5f;

    g.DrawLine(kCenterLineColor, barPlot.L, centerY, barPlot.R, centerY, &mBlend, 1.f);

    for(const auto& osc : mFrame.harmonics)
    {
      if(osc.frequencyHz < kMinFrequencyHzX || osc.frequencyHz > kMaxFrequencyHzX || osc.level <= 0.f)
        continue;

      const float colorT = FrequencyToNormalizedLogColorPosition(osc.frequencyHz);
      const IColor harmonicColor = LerpColor(kHarmonicColorStart, kHarmonicColorEnd, colorT);
      const IColor glowOuterColor = harmonicColor.WithOpacity(0.40f);
      const IColor glowInnerColor = harmonicColor.WithOpacity(1.0f);
      const IColor coreLineColor = kWhite;

      const float x = FrequencyToX(osc.frequencyHz, barPlot);
      const float mappedLevel = harmonic_visualizer_level::MapPseudoLog(osc.level);

      const float leftNormHeight = std::min(1.f, mappedLevel * std::clamp(osc.panLeftGain, 0.f, 1.f) * kYAxisBoost);
      const float rightNormHeight = std::min(1.f, mappedLevel * std::clamp(osc.panRightGain, 0.f, 1.f) * kYAxisBoost);
      const float leftHeight = leftNormHeight * channelHeight;
      const float rightHeight = rightNormHeight * channelHeight;

      if(leftHeight > 0.f)
      {
        DrawGlowingVerticalLine(
          g,
          x,
          centerY - leftHeight,
          centerY,
          glowOuterColor,
          glowInnerColor,
          coreLineColor,
          glowOuterBlend,
          glowInnerBlend,
          coreBlend,
          kGlowOuterThickness,
          kGlowInnerThickness,
          kCoreThickness);
      }

      if(rightHeight > 0.f)
      {
        DrawGlowingVerticalLine(
          g,
          x,
          centerY,
          centerY + rightHeight,
          glowOuterColor,
          glowInnerColor,
          coreLineColor,
          glowOuterBlend,
          glowInnerBlend,
          coreBlend,
          kGlowOuterThickness,
          kGlowInnerThickness,
          kCoreThickness);
      }
    }

    DrawFrequencyLabel(g, labelText, "100 Hz", 100.f, labelArea, barPlot);
    DrawFrequencyLabel(g, labelText, "1 kHz", 1000.f, labelArea, barPlot);
    DrawFrequencyLabel(g, labelText, "10 kHz", 10000.f, labelArea, barPlot);
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if(IsDisabled() || msgTag != ISender<>::kUpdateMessage)
      return;

    IByteStream stream(pData, dataSize);
    ISenderData<1, HarmonicVisualizerFrame> data;
    int pos = 0;
    pos = stream.Get(&data, pos);

    if(pos >= 0)
    {
      mFrame = data.vals[0];
      SetDirty(false);
    }
  }

private:
  static IColor LerpColor(const IColor& a, const IColor& b, float t)
  {
    const float clampedT = std::clamp(t, 0.f, 1.f);

    const auto lerpChannel = [clampedT](int x, int y) {
      return static_cast<int>(std::lround(static_cast<double>(x) + static_cast<double>(y - x) * static_cast<double>(clampedT)));
    };

    return IColor(
      lerpChannel(a.A, b.A),
      lerpChannel(a.R, b.R),
      lerpChannel(a.G, b.G),
      lerpChannel(a.B, b.B));
  }

  static void DrawGlowingVerticalLine(
    IGraphics& g,
    float x,
    float y0,
    float y1,
    const IColor& glowOuterColor,
    const IColor& glowInnerColor,
    const IColor& coreColor,
    const IBlend& glowOuterBlend,
    const IBlend& glowInnerBlend,
    const IBlend& coreBlend,
    float glowOuterThickness,
    float glowInnerThickness,
    float coreThickness)
  {
    if(std::fabs(y1 - y0) <= 0.1f)
      return;

    g.DrawLine(glowOuterColor, x, y0, x, y1, &glowOuterBlend, glowOuterThickness);
    g.DrawLine(glowInnerColor, x, y0, x, y1, &glowInnerBlend, glowInnerThickness);
    g.DrawLine(coreColor, x, y0, x, y1, &coreBlend, coreThickness);
  }

  static bool IsLabelFrequency(float frequencyHz)
  {
    return frequencyHz == 100.f || frequencyHz == 1000.f || frequencyHz == 10000.f;
  }

  static void DrawFrequencyLabel(IGraphics& g, const IText& text, const char* label, float frequencyHz, const IRECT& labelArea, const IRECT& plotArea)
  {
    constexpr float kLabelWidth = 68.f;

    const float x = FrequencyToX(frequencyHz, plotArea);
    const IRECT labelBounds{
      x - (kLabelWidth * 0.5f),
      labelArea.T,
      x + (kLabelWidth * 0.5f),
      labelArea.B};

    g.DrawText(text, label, labelBounds, nullptr);
  }

  static float FrequencyToX(float frequencyHz, const IRECT& bounds)
  {
    const float xNorm = FrequencyToNormalizedLogXPosition(frequencyHz);
    return bounds.L + (xNorm * bounds.W());
  }

  static float FrequencyToNormalizedLogXPosition(float frequencyHz)
  {
    const float clampedFrequency = std::clamp(frequencyHz, kMinFrequencyHzX, kMaxFrequencyHzX);
    const float logMin = std::log(kMinFrequencyHzX);
    const float logMax = std::log(kMaxFrequencyHzX);
    return (std::log(clampedFrequency) - logMin) / (logMax - logMin);
  }

  static float FrequencyToNormalizedLogColorPosition(float frequencyHz)
  {
    const float clampedFrequency = std::clamp(frequencyHz, kMinFrequencyHzColor, kMaxFrequencyHzColor);
    const float logMin = std::log(kMinFrequencyHzColor);
    const float logMax = std::log(kMaxFrequencyHzColor);
    return (std::log(clampedFrequency) - logMin) / (logMax - logMin);
  }

  static constexpr float kMinFrequencyHzX = 20.f;
  static constexpr float kMaxFrequencyHzX = 20000.f;
  static constexpr float kMinFrequencyHzColor = 100.f;
  static constexpr float kMaxFrequencyHzColor = 10000.f;
  static constexpr std::array<float, 28> kGridFrequenciesHz{
    20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f, 100.f,
    200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f, 900.f, 1000.f,
    2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f, 10000.f, 20000.f};
  HarmonicVisualizerFrame mFrame{};
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE

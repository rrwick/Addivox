#pragma once

#include "IControl.h"
#include "IPlugStructs.h"
#include "ISender.h"
#include "../ui/colour.h"
#include "harmonic_visualizer_frame.h"
#include "../ui/transformations.h"

#include <algorithm>
#include <array>
#include <cmath>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

namespace harmonic_visualizer_level
{
inline float MapPseudoLog(float level)
{
  // Use shared pseudo-log inverse mapping for display compression.
  constexpr double kShape = 4.174387269895637; // ln(65), equivalent to prior log1p(k=64) mapping.
  return static_cast<float>(transformations::NormalizedExpInverse(level, kShape));
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
    constexpr float kCornerRadius = 7.f;
    constexpr float kLabelAreaHeight = 18.f;

    g.FillRoundRect(plugin_ui::colour::visualizer::kBackground, mRECT, kCornerRadius, &mBlend);
    g.DrawRoundRect(plugin_ui::colour::visualizer::kFrame, mRECT, kCornerRadius, &mBlend, 1.f);

    const IRECT plot = mRECT.GetPadded(-2.f);
    const IRECT barPlot{plot.L, plot.T, plot.R, plot.B - kLabelAreaHeight};
    const IRECT labelArea{plot.L, barPlot.B, plot.R, plot.B};
    const IText labelText{11.f, plugin_ui::colour::visualizer::kLabelText, "Roboto-Regular", EAlign::Center, EVAlign::Bottom};
    const auto& xMapping = XFrequencyMapping();
    const auto& colorMapping = ColorFrequencyMapping();
    const float baseBlendWeight = BlendWeight(&mBlend);
    const float centerY = barPlot.MH();
    const float channelHeight = (barPlot.H() * 0.5f) - 1.f;

    auto drawBarSegment = [&](float x, float y0, float y1, const IColor& harmonicColor, float height) {
      if(height <= 0.f)
        return;

      DrawGlowingVerticalLine(
        g,
        x,
        y0,
        y1,
        harmonicColor,
        plugin_ui::colour::visualizer::kHarmonicCore,
        baseBlendWeight,
        VisibilityForHeightPixels(height));
    };

    for(const float frequencyHz : kGridFrequenciesHz)
    {
      const float x = LerpXFromNormalized(xMapping.Normalize(frequencyHz), barPlot);
      const bool major = IsMajorGridFrequency(frequencyHz);
      g.DrawLine(
        major ? plugin_ui::colour::visualizer::kGridMajor : plugin_ui::colour::visualizer::kGridMinor,
        x,
        barPlot.T,
        x,
        barPlot.B,
        &mBlend,
        major ? 1.1f : 1.f);
    }

    constexpr float kYAxisBoost = 1.5f;

    g.DrawLine(plugin_ui::colour::visualizer::kCenterLine, barPlot.L, centerY, barPlot.R, centerY, &mBlend, 1.f);

    for(const auto& osc : mFrame.harmonics)
    {
      if(osc.frequencyHz < kMinFrequencyHzX || osc.frequencyHz > kMaxFrequencyHzX || osc.level <= 0.f)
        continue;

      const float clampedFrequencyHz = std::clamp(osc.frequencyHz, kMinFrequencyHzX, kMaxFrequencyHzX);
      const float logFrequency = std::log(clampedFrequencyHz);
      const float xNorm = xMapping.NormalizeFromLog(logFrequency);
      const float colorT = colorMapping.NormalizeFromLog(logFrequency);
      const IColor harmonicColor = LerpColor(plugin_ui::colour::visualizer::kHarmonicGradientStart, plugin_ui::colour::visualizer::kHarmonicGradientEnd, colorT);
      const float x = LerpXFromNormalized(xNorm, barPlot);
      const float mappedLevel = harmonic_visualizer_level::MapPseudoLog(osc.level);

      const float leftNormHeight = std::min(1.f, mappedLevel * osc.panLeftGain * kYAxisBoost);
      const float rightNormHeight = std::min(1.f, mappedLevel * osc.panRightGain * kYAxisBoost);
      const float leftHeight = leftNormHeight * channelHeight;
      const float rightHeight = rightNormHeight * channelHeight;

      drawBarSegment(x, centerY - leftHeight, centerY, harmonicColor, leftHeight);
      drawBarSegment(x, centerY, centerY + rightHeight, harmonicColor, rightHeight);
    }

    for(std::size_t i = 0; i < kAxisLabelFrequenciesHz.size(); ++i)
      DrawFrequencyLabel(g, labelText, kAxisLabelStrings[i], kAxisLabelFrequenciesHz[i], labelArea, barPlot, xMapping);
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
  struct LogFrequencyMapping
  {
    float minHz;
    float maxHz;
    float logMin;
    float invLogSpan;

    float Normalize(float frequencyHz) const
    {
      return NormalizeFromLog(std::log(std::clamp(frequencyHz, minHz, maxHz)));
    }

    float NormalizeFromLog(float logFrequency) const
    {
      return std::clamp((logFrequency - logMin) * invLogSpan, 0.f, 1.f);
    }
  };

  static const LogFrequencyMapping& XFrequencyMapping()
  {
    static const LogFrequencyMapping mapping = [] {
      const float logMin = std::log(kMinFrequencyHzX);
      const float logMax = std::log(kMaxFrequencyHzX);
      return LogFrequencyMapping{kMinFrequencyHzX, kMaxFrequencyHzX, logMin, 1.f / (logMax - logMin)};
    }();
    return mapping;
  }

  static const LogFrequencyMapping& ColorFrequencyMapping()
  {
    static const LogFrequencyMapping mapping = [] {
      const float logMin = std::log(kMinFrequencyHzColor);
      const float logMax = std::log(kMaxFrequencyHzColor);
      return LogFrequencyMapping{kMinFrequencyHzColor, kMaxFrequencyHzColor, logMin, 1.f / (logMax - logMin)};
    }();
    return mapping;
  }

  static float LerpXFromNormalized(float xNorm, const IRECT& bounds)
  {
    return bounds.L + (xNorm * bounds.W());
  }

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

  static const IStrokeOptions& RoundCapStrokeOptions()
  {
    static const IStrokeOptions options = [] {
      IStrokeOptions strokeOptions;
      strokeOptions.mCapOption = ELineCap::Round;
      strokeOptions.mJoinOption = ELineJoin::Round;
      return strokeOptions;
    }();
    return options;
  }

  static void DrawRoundedVerticalStroke(IGraphics& g, float x, float y0, float y1, const IColor& color, float thickness, const IBlend& blend)
  {
    g.PathMoveTo(x, y0);
    g.PathLineTo(x, y1);
    g.PathStroke(color, thickness, RoundCapStrokeOptions(), &blend);
  }

  static float Smoothstep(float edge0, float edge1, float x)
  {
    if(edge1 <= edge0)
      return (x >= edge1) ? 1.f : 0.f;

    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
    t = std::sqrt(t);
    return t * t * (3.f - 2.f * t);
  }

  static float VisibilityForHeightPixels(float heightPixels)
  {
    // Very short lines fade out to reduce clutter in dense high-harmonic tails.
    constexpr float kFadeStartPx = 0.f;
    constexpr float kFadeEndPx = 5.f;
    const float h = std::max(0.f, heightPixels);
    if(h <= kFadeStartPx)
      return 0.f;
    if(h >= kFadeEndPx)
      return 1.f;

    return Smoothstep(kFadeStartPx, kFadeEndPx, h);
  }

  static void DrawGlowingVerticalLine(
    IGraphics& g,
    float x,
    float y0,
    float y1,
    const IColor& glowColor,
    const IColor& coreColor,
    float baseBlendWeight,
    float visibility)
  {
    if(std::fabs(y1 - y0) <= 0.1f)
      return;

    const float clampedVisibility = std::clamp(visibility, 0.f, 1.f);
    if(clampedVisibility <= 0.f)
      return;

    // 5-pass stack: 4 additive glow layers + 1 bright core.
    for(std::size_t pass = 0; pass < kGlowThicknesses.size(); ++pass)
    {
      const IColor passColor = glowColor.WithOpacity(kGlowOpacities[pass] * clampedVisibility);
      const IBlend passBlend{EBlend::Add, kGlowBlendWeights[pass] * baseBlendWeight * clampedVisibility};
      DrawRoundedVerticalStroke(g, x, y0, y1, passColor, kGlowThicknesses[pass], passBlend);
    }

    const IColor coreColorScaled = coreColor.WithOpacity(clampedVisibility);
    const IBlend coreBlend{EBlend::SrcOver, baseBlendWeight * clampedVisibility};
    DrawRoundedVerticalStroke(g, x, y0, y1, coreColorScaled, kCoreThicknessPx, coreBlend);
  }

  static bool IsMajorGridFrequency(float frequencyHz)
  {
    for(float majorFrequency : kAxisLabelFrequenciesHz)
    {
      if(majorFrequency == frequencyHz)
        return true;
    }
    return false;
  }

  static void DrawFrequencyLabel(
    IGraphics& g,
    const IText& text,
    const char* label,
    float frequencyHz,
    const IRECT& labelArea,
    const IRECT& plotArea,
    const LogFrequencyMapping& xMapping)
  {
    constexpr float kLabelWidth = 68.f;

    const float x = LerpXFromNormalized(xMapping.Normalize(frequencyHz), plotArea);
    const IRECT labelBounds{
      x - (kLabelWidth * 0.5f),
      labelArea.T,
      x + (kLabelWidth * 0.5f),
      labelArea.B};

    g.DrawText(text, label, labelBounds, nullptr);
  }

  static constexpr float kMinFrequencyHzX = 20.f;
  static constexpr float kMaxFrequencyHzX = 20000.f;
  static constexpr float kMinFrequencyHzColor = 100.f;
  static constexpr float kMaxFrequencyHzColor = 10000.f;
  static constexpr float kCoreThicknessPx = 1.0f;
  static constexpr std::array<float, 3> kAxisLabelFrequenciesHz{100.f, 1000.f, 10000.f};
  static constexpr std::array<const char*, 3> kAxisLabelStrings{"100 Hz", "1 kHz", "10 kHz"};
  static constexpr std::array<float, 4> kGlowThicknesses{12.f, 8.f, 5.f, 3.0f};
  static constexpr std::array<float, 4> kGlowOpacities{0.5f, 0.7f, 0.9f, 1.0f};
  static constexpr std::array<float, 4> kGlowBlendWeights{0.5f, 0.7f, 0.9f, 1.0f};
  static constexpr std::array<float, 28> kGridFrequenciesHz{
    20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f, 100.f,
    200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f, 900.f, 1000.f,
    2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f, 10000.f, 20000.f};
  HarmonicVisualizerFrame mFrame{};
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE

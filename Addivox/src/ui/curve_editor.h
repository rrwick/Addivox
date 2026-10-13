#pragma once

#include "../settings/eq.h"
#include "IControls.h"
#include "colour.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

// Shared look and coordinate mapping for editors that draw gain curves over the log-frequency axis, on EqCurve's visual-y gain scale.
class CurveEditorControl : public IControl {
public:
  using IControl::IControl;

  void OnAttached() override {
    IControl::OnAttached();
    if (mDoubleTapGestureAttached || !GetUI()) return;

    AttachGestureRecognizer(EGestureType::DoubleTap, [this](IControl*, const IGestureInfo& info) { TogglePointAt(info.x, info.y); });
    mDoubleTapGestureAttached = true;
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override {
    IControl::OnMouseDblClick(x, y, mod);
    TogglePointAt(x, y);
  }

protected:
  // Double-click or double-tap: removes the point under (x, y), or adds one there.
  virtual void TogglePointAt(float x, float y) = 0;

  static constexpr int kNoPointIndex = -1;
  static constexpr float kPointRadiusPx = 5.f;
  static constexpr float kCurveThicknessPx = 2.0f;

  static float NormalizedXFromFrequency(double frequencyHz) {
    const double clampedFrequencyHz = std::clamp(frequencyHz, EqCurve::kMinFrequencyHz, EqCurve::kMaxFrequencyHz);
    const double logMin = std::log(EqCurve::kMinFrequencyHz);
    const double logMax = std::log(EqCurve::kMaxFrequencyHz);
    return static_cast<float>((std::log(clampedFrequencyHz) - logMin) / (logMax - logMin));
  }

  static double FrequencyFromNormalizedX(float xNorm) {
    const double logMin = std::log(EqCurve::kMinFrequencyHz);
    const double logMax = std::log(EqCurve::kMaxFrequencyHz);
    const double clampedNorm = std::clamp(static_cast<double>(xNorm), 0.0, 1.0);
    return std::exp(logMin + ((logMax - logMin) * clampedNorm));
  }

  static float XFromFrequency(double frequencyHz, const IRECT& plotBounds) { return plotBounds.L + (NormalizedXFromFrequency(frequencyHz) * plotBounds.W()); }

  static double FrequencyFromX(float x, const IRECT& plotBounds) {
    const float clampedX = std::clamp(x, plotBounds.L, plotBounds.R);
    const float xNorm = (plotBounds.W() <= 0.f) ? 0.f : ((clampedX - plotBounds.L) / plotBounds.W());
    return FrequencyFromNormalizedX(xNorm);
  }

  static float YFromVisualY(double visualY, const IRECT& plotBounds) { return plotBounds.T + (static_cast<float>(visualY) * plotBounds.H()); }

  static float YFromDb(double gainDb, const IRECT& plotBounds) { return YFromVisualY(EqCurve::VisualYFromDb(gainDb), plotBounds); }

  static double GainDbFromY(float y, const IRECT& plotBounds) {
    return plotBounds.H() <= 0.f ? EqCurve::kMaxGainDb : EqCurve::DbFromVisualY((y - plotBounds.T) / plotBounds.H());
  }

  IRECT GetPlotBounds() const {
    constexpr float kOuterInset = 2.f;
    constexpr float kLeftLabelWidth = 40.f;
    constexpr float kBottomLabelHeight = 18.f;

    const IRECT innerBounds = mRECT.GetPadded(-kOuterInset);
    return IRECT(innerBounds.L + kLeftLabelWidth, innerBounds.T + 8.f, innerBounds.R - 8.f, innerBounds.B - kBottomLabelHeight);
  }

  void DrawBackgroundAndGrid(IGraphics& g, const IRECT& plotBounds) const {
    constexpr float kCornerRadius = 7.f;

    g.FillRoundRect(colour::visualizer::kBackground, mRECT, kCornerRadius, &mBlend);
    g.DrawRoundRect(colour::visualizer::kFrame, mRECT, kCornerRadius, &mBlend, 1.f);

    for (const float frequencyHz : kGridFrequenciesHz) {
      const float x = XFromFrequency(frequencyHz, plotBounds);
      const IColor color = IsMajorGridFrequency(frequencyHz) ? colour::visualizer::kGridMajor : colour::visualizer::kGridMinor;
      g.DrawLine(color, x, plotBounds.T, x, plotBounds.B, &mBlend, 1.f);
    }

    for (const double gainDb : kYAxisDbLabels) {
      const float y = YFromDb(gainDb, plotBounds);
      const bool isZeroDb = std::fabs(gainDb) < 0.01f;
      const IColor color = isZeroDb ? colour::visualizer::kCenterLine : colour::visualizer::kGridMinor;
      g.DrawLine(color, plotBounds.L, y, plotBounds.R, y, &mBlend, isZeroDb ? 1.1f : 1.f);
    }
  }

  void DrawAxisLabels(IGraphics& g, const IRECT& plotBounds) const {
    const IText axisText{11.f, colour::visualizer::kLabelText, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
    const IRECT gainLabelBounds{mRECT.L + 4.f, plotBounds.T, plotBounds.L - 6.f, plotBounds.B};
    const IRECT frequencyLabelBounds{plotBounds.L, plotBounds.B + 2.f, plotBounds.R, mRECT.B - 2.f};

    for (std::size_t i = 0; i < kYAxisDbLabels.size(); ++i) {
      const float y = YFromDb(kYAxisDbLabels[i], plotBounds);
      g.DrawText(axisText, kYAxisLabelStrings[i], IRECT(gainLabelBounds.L, y - 7.f, gainLabelBounds.R, y + 7.f), nullptr);
    }

    for (std::size_t i = 0; i < kFrequencyLabelsHz.size(); ++i) {
      const float x = XFromFrequency(kFrequencyLabelsHz[i], plotBounds);
      g.DrawText(axisText, kFrequencyLabelStrings[i], IRECT(x - 30.f, frequencyLabelBounds.T, x + 30.f, frequencyLabelBounds.B), nullptr);
    }
  }

  // Strokes evaluateVisualY(frequencyHz) across the plot, one sample per pixel. A bold curve gets a wider stroke underneath in the same colour, so it looks
  // solid, or two-tone when the control is greyed out (between key notes).
  template <typename EvaluateVisualY>
  void DrawCurve(IGraphics& g, const IRECT& plotBounds, EvaluateVisualY&& evaluateVisualY, const IColor& color, bool bold) const {
    const int numSamples = std::max(2, static_cast<int>(std::ceil(plotBounds.W())));
    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
      const float t = static_cast<float>(sampleIndex) / static_cast<float>(numSamples - 1);
      const float x = plotBounds.L + (t * plotBounds.W());
      const float y = YFromVisualY(evaluateVisualY(FrequencyFromX(x, plotBounds)), plotBounds);
      if (sampleIndex == 0) g.PathMoveTo(x, y);
      else
        g.PathLineTo(x, y);
    }

    IStrokeOptions strokeOptions;
    strokeOptions.mCapOption = ELineCap::Round;
    strokeOptions.mJoinOption = ELineJoin::Round;
    if (bold) {
      strokeOptions.mPreserve = true;
      g.PathStroke(color, kCurveThicknessPx + 3.f, strokeOptions, &mBlend);
      strokeOptions.mPreserve = false;
    }
    g.PathStroke(color, kCurveThicknessPx, strokeOptions, &mBlend);
  }

  void DrawPoints(IGraphics& g, const IRECT& plotBounds, const EqCurve::PointList& points, const IColor& fill, const IColor& outline,
                  int activePointIndex = kNoPointIndex) const {
    for (std::size_t i = 0; i < points.size(); ++i) {
      const float x = XFromFrequency(points[i].frequencyHz, plotBounds);
      const float y = YFromDb(points[i].gainDb, plotBounds);
      const bool active = static_cast<int>(i) == activePointIndex;
      const float radius = active ? (kPointRadiusPx + 1.f) : kPointRadiusPx;
      g.FillCircle(active ? colour::ui::kAccentSecondary : fill, x, y, radius, &mBlend);
      g.DrawCircle(outline, x, y, radius, &mBlend, 1.f);
    }
  }

  void DrawValueBubble(IGraphics& g, const IRECT& plotBounds, const EqPoint& point) const {
    constexpr float kBubbleCornerRadius = 6.f;
    constexpr float kBubblePaddingX = 8.f;
    constexpr float kBubblePaddingY = 4.f;
    constexpr float kBubblePointGap = 10.f;
    constexpr float kBubbleOuterInset = 4.f;
    constexpr float kBubbleLineGap = 2.f;

    const float pointX = XFromFrequency(point.frequencyHz, plotBounds);
    const float pointY = YFromDb(point.gainDb, plotBounds);
    const std::string frequencyLabel = FormatFrequency(point.frequencyHz);
    const std::string gainLabel = FormatGainDb(point.gainDb);

    const IText bubbleText{15.f, colour::ui::kValueText, "Roboto-Regular", EAlign::Center, EVAlign::Middle};

    IRECT frequencyTextBounds;
    IRECT gainTextBounds;
    g.MeasureText(bubbleText, frequencyLabel.c_str(), frequencyTextBounds);
    g.MeasureText(bubbleText, gainLabel.c_str(), gainTextBounds);

    const float lineWidth = std::max({frequencyTextBounds.W(), gainTextBounds.W(), 1.f});
    const float lineHeight = std::max({frequencyTextBounds.H(), gainTextBounds.H(), bubbleText.mSize});
    const float bubbleWidth = lineWidth + (kBubblePaddingX * 2.f);
    const float bubbleHeight = (lineHeight * 2.f) + kBubbleLineGap + (kBubblePaddingY * 2.f);
    const float minBubbleL = plotBounds.L + kBubbleOuterInset;
    const float maxBubbleL = plotBounds.R - kBubbleOuterInset - bubbleWidth;
    const float bubbleL = std::clamp(pointX - (bubbleWidth * 0.5f), minBubbleL, maxBubbleL);
    const float bubbleR = bubbleL + bubbleWidth;

    float bubbleB = pointY - (kPointRadiusPx + kBubblePointGap);
    float bubbleT = bubbleB - bubbleHeight;
    if (bubbleT < plotBounds.T + kBubbleOuterInset) {
      bubbleT = pointY + kPointRadiusPx + kBubblePointGap;
      bubbleB = bubbleT + bubbleHeight;
    }

    const IRECT bubbleBounds{bubbleL, bubbleT, bubbleR, bubbleB};
    g.FillRoundRect(colour::editor::kUtilityBody, bubbleBounds, kBubbleCornerRadius, &mBlend);
    g.DrawRoundRect(colour::ui::kControlFrame, bubbleBounds, kBubbleCornerRadius, &mBlend, 1.f);

    const float contentTop = bubbleBounds.T + kBubblePaddingY;
    const IRECT frequencyLineBounds{bubbleBounds.L + kBubblePaddingX, contentTop, bubbleBounds.R - kBubblePaddingX, contentTop + lineHeight};
    const IRECT gainLineBounds{bubbleBounds.L + kBubblePaddingX, frequencyLineBounds.B + kBubbleLineGap, bubbleBounds.R - kBubblePaddingX,
                               frequencyLineBounds.B + kBubbleLineGap + lineHeight};

    g.DrawText(bubbleText, frequencyLabel.c_str(), frequencyLineBounds, nullptr);
    g.DrawText(bubbleText, gainLabel.c_str(), gainLineBounds, nullptr);
  }

  int HitTestPoint(const EqCurve::PointList& points, float x, float y) const {
    const IRECT plotBounds = GetPlotBounds();
    const double maxDistanceSquared = static_cast<double>(kPointRadiusPx + 3.f) * static_cast<double>(kPointRadiusPx + 3.f);
    int hitPointIndex = kNoPointIndex;
    double closestDistanceSquared = std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < points.size(); ++i) {
      const double dx = static_cast<double>(XFromFrequency(points[i].frequencyHz, plotBounds) - x);
      const double dy = static_cast<double>(YFromDb(points[i].gainDb, plotBounds) - y);
      const double distanceSquared = (dx * dx) + (dy * dy);
      if (distanceSquared <= maxDistanceSquared && distanceSquared < closestDistanceSquared) {
        closestDistanceSquared = distanceSquared;
        hitPointIndex = static_cast<int>(i);
      }
    }

    return hitPointIndex;
  }

private:
  bool mDoubleTapGestureAttached{false};

  static constexpr std::array<float, 28> kGridFrequenciesHz{20.f,   30.f,   40.f,   50.f,   60.f,   70.f,   80.f,    90.f,   100.f,  200.f,
                                                            300.f,  400.f,  500.f,  600.f,  700.f,  800.f,  900.f,   1000.f, 2000.f, 3000.f,
                                                            4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f, 10000.f, 20000.f};

  static constexpr std::array<float, 13> kFrequencyLabelsHz{20.f, 30.f, 50.f, 100.f, 200.f, 300.f, 500.f, 1000.f, 2000.f, 3000.f, 5000.f, 10000.f, 20000.f};
  static constexpr std::array<const char*, 13> kFrequencyLabelStrings{"20 Hz", "30 Hz", "50 Hz", "100 Hz", "200 Hz", "300 Hz", "500 Hz",
                                                                      "1 kHz", "2 kHz", "3 kHz", "5 kHz",  "10 kHz", "20 kHz"};

  static constexpr std::array<double, 6> kYAxisDbLabels{EqCurve::kMinGainDb, EqCurve::kMinFiniteGainDb, -12.0, 0.0, 12.0, 24.0};
  static constexpr std::array<const char*, 6> kYAxisLabelStrings{"mute", "-24", "-12", "0", "+12", "+24"};

  static bool IsMajorGridFrequency(float frequencyHz) {
    const float log10 = std::log10(frequencyHz);
    return std::fabs(log10 - std::round(log10)) < 0.01;
  }

  static std::string FormatFrequency(double frequencyHz) {
    std::array<char, 32> buffer{};
    if (frequencyHz < 1000.0) {
      std::snprintf(buffer.data(), buffer.size(), "%d Hz", static_cast<int>(std::lround(frequencyHz)));
    } else {
      std::snprintf(buffer.data(), buffer.size(), "%.3g kHz", frequencyHz / 1000.0);
    }

    return std::string{buffer.data()};
  }

  static std::string FormatGainDb(double gainDb) {
    if (EqCurve::IsMutedGainDb(gainDb)) return "mute";

    const double roundedGainDb = std::round(gainDb * 10.0) / 10.0;
    const double normalizedGainDb = (std::abs(roundedGainDb) < 0.05) ? 0.0 : roundedGainDb;

    std::array<char, 24> buffer{};
    std::snprintf(buffer.data(), buffer.size(), normalizedGainDb > 0.0 ? "+%.1f dB" : "%.1f dB", normalizedGainDb);

    return std::string{buffer.data()};
  }
};
} // namespace plugin_ui

#pragma once

#include "IControls.h"
#include "colour.h"
#include "../settings/eq.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

class EqEditorControl final : public IControl
{
public:
  using OnCurveChangedFunc = std::function<void(const EqCurve&)>;

  explicit EqEditorControl(const IRECT& bounds)
  : IControl(bounds)
  {
  }

  void SetCurve(const EqCurve& curve)
  {
    mCurve = curve;
    if(mDraggedPointIndex != kNoPointIndex
       && (mDraggedPointIndex < 0
           || static_cast<std::size_t>(mDraggedPointIndex) >= mCurve.GetPoints().size()))
    {
      mDraggedPointIndex = kNoPointIndex;
    }
    SetDirty(false);
  }

  const EqCurve& GetCurve() const
  {
    return mCurve;
  }

  void SetEditable(bool editable)
  {
    SetDisabled(!editable);
    if(!editable)
      mDraggedPointIndex = kNoPointIndex;
    SetDirty(false);
  }

  bool IsEditable() const
  {
    return !IsDisabled();
  }

  void SetOnCurveChanged(OnCurveChangedFunc func)
  {
    mOnCurveChanged = std::move(func);
  }

  void CaptureRestoreState(int midiNote)
  {
    mRestoreCurve = mCurve;
    mHasRestoreState = true;
    mRestoreMidiNote = midiNote;
  }

  void ClearRestoreState()
  {
    mHasRestoreState = false;
    mRestoreMidiNote = kNoRestoreMidiNote;
    mRestoreCurve = EqCurve{};
  }

  bool HasRestoreState() const
  {
    return mHasRestoreState;
  }

  bool HasRestoreStateForMidiNote(int midiNote) const
  {
    return mHasRestoreState && (mRestoreMidiNote == midiNote);
  }

  int GetRestoreMidiNote() const
  {
    return mRestoreMidiNote;
  }

  const EqCurve& GetRestoreState() const
  {
    return mRestoreCurve;
  }

  void Draw(IGraphics& g) override
  {
    constexpr float kCornerRadius = 7.f;

    g.FillRoundRect(colour::visualizer::kBackground, mRECT, kCornerRadius, &mBlend);
    g.DrawRoundRect(colour::visualizer::kFrame, mRECT, kCornerRadius, &mBlend, 1.f);

    const IRECT plotBounds = GetPlotBounds();
    const IRECT gainLabelBounds = GetGainLabelBounds();
    const IRECT frequencyLabelBounds = GetFrequencyLabelBounds();
    const IText axisText{11.f, colour::visualizer::kLabelText, "Roboto-Regular", EAlign::Center, EVAlign::Middle};

    DrawFrequencyGrid(g, plotBounds);
    DrawGainGrid(g, plotBounds);
    DrawCurve(g, plotBounds);

    if(IsEditable())
      DrawPoints(g, plotBounds);

    for(std::size_t i = 0; i < kYAxisDbLabels.size(); ++i)
    {
      const float y = YFromDb(kYAxisDbLabels[i], plotBounds);
      g.DrawText(
        axisText,
        kYAxisLabelStrings[i],
        IRECT(gainLabelBounds.L, y - 7.f, gainLabelBounds.R, y + 7.f),
        nullptr);
    }

    for(std::size_t i = 0; i < kFrequencyLabelsHz.size(); ++i)
    {
      const float x = XFromFrequency(kFrequencyLabelsHz[i], plotBounds);
      g.DrawText(
        axisText,
        kFrequencyLabelStrings[i],
        IRECT(x - 30.f, frequencyLabelBounds.T, x + 30.f, frequencyLabelBounds.B),
        nullptr);
    }

    if(IsEditable())
      DrawDraggedPointValueBubble(g, plotBounds);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    IControl::OnMouseDown(x, y, mod);

    if(!IsEditable())
      return;

    mDraggedPointIndex = HitTestPoint(x, y);
    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    if(!IsEditable() || mDraggedPointIndex == kNoPointIndex)
      return;

    auto points = mCurve.GetPoints();
    if(mDraggedPointIndex < 0 || static_cast<std::size_t>(mDraggedPointIndex) >= points.size())
      return;

    const IRECT plotBounds = GetPlotBounds();
    double xNorm = NormalizedXFromFrequency(FrequencyFromX(x, plotBounds));
    if(mDraggedPointIndex > 0)
    {
      const float minNorm = NormalizedXFromFrequency(points[static_cast<std::size_t>(mDraggedPointIndex - 1)].frequencyHz) + kMinPointSeparationNorm;
      xNorm = std::max(xNorm, static_cast<double>(minNorm));
    }

    if(static_cast<std::size_t>(mDraggedPointIndex + 1) < points.size())
    {
      const float maxNorm = NormalizedXFromFrequency(points[static_cast<std::size_t>(mDraggedPointIndex + 1)].frequencyHz) - kMinPointSeparationNorm;
      xNorm = std::min(xNorm, static_cast<double>(maxNorm));
    }

    points[static_cast<std::size_t>(mDraggedPointIndex)] = {
      FrequencyFromNormalizedX(static_cast<float>(xNorm)),
      GainDbFromY(y, plotBounds)};
    mCurve.SetPoints(std::move(points));
    NotifyCurveChanged();
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    IControl::OnMouseUp(x, y, mod);
    mDraggedPointIndex = kNoPointIndex;
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    IControl::OnMouseDblClick(x, y, mod);

    if(!IsEditable())
      return;

    auto points = mCurve.GetPoints();
    const int hitPointIndex = HitTestPoint(x, y);
    if(hitPointIndex >= 0 && static_cast<std::size_t>(hitPointIndex) < points.size())
    {
      points.erase(points.begin() + hitPointIndex);
      mCurve.SetPoints(std::move(points));
      NotifyCurveChanged();
      return;
    }

    const IRECT plotBounds = GetPlotBounds();
    points.push_back({
      FrequencyFromX(x, plotBounds),
      GainDbFromY(y, plotBounds)});
    mCurve.SetPoints(std::move(points));
    NotifyCurveChanged();
  }

private:
  static constexpr int kNoPointIndex = -1;
  static constexpr float kPointRadiusPx = 5.f;
  static constexpr float kCurveThicknessPx = 2.0f;
  static constexpr float kMinPointSeparationNorm = 0.0025f;
  static constexpr int kNoRestoreMidiNote = std::numeric_limits<int>::min();
  static constexpr std::array<float, 28> kGridFrequenciesHz{
    20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f,
    100.f, 200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f, 900.f,
    1000.f, 2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f,
    10000.f, 20000.f};
  static constexpr std::array<float, 13> kFrequencyLabelsHz{20.f, 30.f, 50.f, 100.f, 200.f, 300.f, 500.f, 1000.f, 2000.f, 3000.f, 5000.f, 10000.f, 20000.f};
  static constexpr std::array<const char*, 13> kFrequencyLabelStrings{"20 Hz", "30 Hz", "50 Hz", "100 Hz", "200 Hz", "300 Hz", "500 Hz", "1 kHz", "2 kHz", "3 kHz", "5 kHz", "10 kHz", "20 kHz"};
  
  static constexpr std::array<float, 5> kYAxisDbLabels{-24.f, -12.f, 0.f, 12.f, 24.f};
  static constexpr std::array<const char*, 5> kYAxisLabelStrings{
    "-24", "-12", "0", "+12", "+24"};

  static bool IsMajorGridFrequency(float frequencyHz)
  {
    // Returns true if the frequency is a power of 10.
    const float log10 = std::log10(frequencyHz);
    return std::fabs(log10 - std::round(log10)) < 0.01;
  }

  static float NormalizedXFromFrequency(double frequencyHz)
  {
    const double clampedFrequencyHz = std::clamp(frequencyHz, EqCurve::kMinFrequencyHz, EqCurve::kMaxFrequencyHz);
    const double logMin = std::log(EqCurve::kMinFrequencyHz);
    const double logMax = std::log(EqCurve::kMaxFrequencyHz);
    return static_cast<float>((std::log(clampedFrequencyHz) - logMin) / (logMax - logMin));
  }

  static double FrequencyFromNormalizedX(float xNorm)
  {
    const double logMin = std::log(EqCurve::kMinFrequencyHz);
    const double logMax = std::log(EqCurve::kMaxFrequencyHz);
    const double clampedNorm = std::clamp(static_cast<double>(xNorm), 0.0, 1.0);
    return std::exp(logMin + ((logMax - logMin) * clampedNorm));
  }

  static float XFromFrequency(double frequencyHz, const IRECT& plotBounds)
  {
    return plotBounds.L + (NormalizedXFromFrequency(frequencyHz) * plotBounds.W());
  }

  static double FrequencyFromX(float x, const IRECT& plotBounds)
  {
    const float clampedX = std::clamp(x, plotBounds.L, plotBounds.R);
    const float xNorm = (plotBounds.W() <= 0.f) ? 0.f : ((clampedX - plotBounds.L) / plotBounds.W());
    return FrequencyFromNormalizedX(xNorm);
  }

  static float YFromDb(double gainDb, const IRECT& plotBounds)
  {
    const double clampedGainDb = std::clamp(gainDb, EqCurve::kMinGainDb, EqCurve::kMaxGainDb);
    const double yNorm =
      (EqCurve::kMaxGainDb - clampedGainDb) / (EqCurve::kMaxGainDb - EqCurve::kMinGainDb);
    return plotBounds.T + (static_cast<float>(yNorm) * plotBounds.H());
  }

  static double GainDbFromY(float y, const IRECT& plotBounds)
  {
    const float clampedY = std::clamp(y, plotBounds.T, plotBounds.B);
    const double yNorm = (plotBounds.H() <= 0.f) ? 0.0 : ((clampedY - plotBounds.T) / plotBounds.H());
    return std::clamp(
      EqCurve::kMaxGainDb - (yNorm * (EqCurve::kMaxGainDb - EqCurve::kMinGainDb)),
      EqCurve::kMinGainDb,
      EqCurve::kMaxGainDb);
  }

  IRECT GetPlotBounds() const
  {
    constexpr float kOuterInset = 2.f;
    constexpr float kLeftLabelWidth = 40.f;
    constexpr float kBottomLabelHeight = 18.f;

    const IRECT innerBounds = mRECT.GetPadded(-kOuterInset);
    return IRECT(
      innerBounds.L + kLeftLabelWidth,
      innerBounds.T + 8.f,
      innerBounds.R - 8.f,
      innerBounds.B - kBottomLabelHeight);
  }

  IRECT GetGainLabelBounds() const
  {
    const IRECT plotBounds = GetPlotBounds();
    return IRECT(mRECT.L + 4.f, plotBounds.T, plotBounds.L - 6.f, plotBounds.B);
  }

  IRECT GetFrequencyLabelBounds() const
  {
    const IRECT plotBounds = GetPlotBounds();
    return IRECT(plotBounds.L, plotBounds.B + 2.f, plotBounds.R, mRECT.B - 2.f);
  }

  void DrawFrequencyGrid(IGraphics& g, const IRECT& plotBounds) const
  {
    for(const float frequencyHz : kGridFrequenciesHz)
    {
      const float x = XFromFrequency(frequencyHz, plotBounds);
      const IColor color = IsMajorGridFrequency(frequencyHz)
        ? colour::visualizer::kGridMajor
        : colour::visualizer::kGridMinor;
      g.DrawLine(color, x, plotBounds.T, x, plotBounds.B, &mBlend, 1.f);
    }
  }

  void DrawGainGrid(IGraphics& g, const IRECT& plotBounds) const
  {
    for(const float gainDb : kYAxisDbLabels)
    {
      const float y = YFromDb(gainDb, plotBounds);
      const IColor color = (std::fabs(gainDb) < 0.01f)
        ? colour::visualizer::kCenterLine
        : colour::visualizer::kGridMinor;
      g.DrawLine(color, plotBounds.L, y, plotBounds.R, y, &mBlend, (std::fabs(gainDb) < 0.01f) ? 1.1f : 1.f);
    }
  }

  void DrawCurve(IGraphics& g, const IRECT& plotBounds) const
  {
    const auto drawSegmentedCurve = [&](const EqCurve& curve, const IColor& color, float thickness) {
      g.PathMoveTo(plotBounds.L, YFromDb(curve.EvaluateDb(EqCurve::kMinFrequencyHz), plotBounds));
      for(const auto& point : curve.GetPoints())
        g.PathLineTo(XFromFrequency(point.frequencyHz, plotBounds), YFromDb(point.gainDb, plotBounds));
      g.PathLineTo(plotBounds.R, YFromDb(curve.EvaluateDb(EqCurve::kMaxFrequencyHz), plotBounds));

      IStrokeOptions strokeOptions;
      strokeOptions.mCapOption = ELineCap::Round;
      strokeOptions.mJoinOption = ELineJoin::Round;
      g.PathStroke(color, thickness, strokeOptions, &mBlend);
    };

    const IColor glowColor = colour::ui::kAccentPrimary.WithOpacity(70);
    const IColor lineColor = colour::ui::kAccentPrimary;
    drawSegmentedCurve(mCurve, glowColor, kCurveThicknessPx + 3.f);
    drawSegmentedCurve(mCurve, lineColor, kCurveThicknessPx);
  }

  void DrawPoints(IGraphics& g, const IRECT& plotBounds) const
  {
    for(std::size_t i = 0; i < mCurve.GetPoints().size(); ++i)
    {
      const auto& point = mCurve.GetPoints()[i];
      const float x = XFromFrequency(point.frequencyHz, plotBounds);
      const float y = YFromDb(point.gainDb, plotBounds);
      const bool active = static_cast<int>(i) == mDraggedPointIndex;
      const float radius = active ? (kPointRadiusPx + 1.f) : kPointRadiusPx;
      g.FillCircle(active ? colour::ui::kAccentSecondary : colour::ui::kAccentPrimary, x, y, radius, &mBlend);
      g.DrawCircle(colour::visualizer::kHarmonicCore, x, y, radius, &mBlend, 1.f);
    }
  }

  static std::string FormatFrequency(double frequencyHz)
  {
    std::array<char, 32> buffer{};
    if(frequencyHz < 1000.0)
    {
      std::snprintf(
        buffer.data(),
        buffer.size(),
        "%d Hz",
        static_cast<int>(std::lround(frequencyHz)));
    }
    else
    {
      std::snprintf(
        buffer.data(),
        buffer.size(),
        "%.3g kHz",
        frequencyHz / 1000.0);
    }

    return std::string{buffer.data()};
  }

  static std::string FormatGainDb(double gainDb)
  {
    const double roundedGainDb = std::round(gainDb * 10.0) / 10.0;
    const double normalizedGainDb = (std::abs(roundedGainDb) < 0.05) ? 0.0 : roundedGainDb;

    std::array<char, 24> buffer{};
    if(normalizedGainDb > 0.0)
      std::snprintf(buffer.data(), buffer.size(), "+%.1f dB", normalizedGainDb);
    else
      std::snprintf(buffer.data(), buffer.size(), "%.1f dB", normalizedGainDb);

    return std::string{buffer.data()};
  }

  void DrawDraggedPointValueBubble(IGraphics& g, const IRECT& plotBounds) const
  {
    if(mDraggedPointIndex == kNoPointIndex)
      return;

    if(mDraggedPointIndex < 0
       || static_cast<std::size_t>(mDraggedPointIndex) >= mCurve.GetPoints().size())
    {
      return;
    }

    constexpr float kBubbleCornerRadius = 6.f;
    constexpr float kBubblePaddingX = 8.f;
    constexpr float kBubblePaddingY = 4.f;
    constexpr float kBubblePointGap = 10.f;
    constexpr float kBubbleOuterInset = 4.f;
    constexpr float kBubbleLineGap = 2.f;

    const auto& point = mCurve.GetPoints()[static_cast<std::size_t>(mDraggedPointIndex)];
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
    if(bubbleT < plotBounds.T + kBubbleOuterInset)
    {
      bubbleT = pointY + kPointRadiusPx + kBubblePointGap;
      bubbleB = bubbleT + bubbleHeight;
    }

    const IRECT bubbleBounds{bubbleL, bubbleT, bubbleR, bubbleB};
    g.FillRoundRect(colour::editor::kUtilityBody, bubbleBounds, kBubbleCornerRadius, &mBlend);
    g.DrawRoundRect(colour::ui::kControlFrame, bubbleBounds, kBubbleCornerRadius, &mBlend, 1.f);

    const float contentTop = bubbleBounds.T + kBubblePaddingY;
    const IRECT frequencyLineBounds{bubbleBounds.L + kBubblePaddingX, contentTop, bubbleBounds.R - kBubblePaddingX, contentTop + lineHeight};
    const IRECT gainLineBounds{bubbleBounds.L + kBubblePaddingX, frequencyLineBounds.B + kBubbleLineGap, bubbleBounds.R - kBubblePaddingX, frequencyLineBounds.B + kBubbleLineGap + lineHeight};

    g.DrawText(bubbleText, frequencyLabel.c_str(), frequencyLineBounds, nullptr);
    g.DrawText(bubbleText, gainLabel.c_str(), gainLineBounds, nullptr);
  }

  int HitTestPoint(float x, float y) const
  {
    if(mCurve.Empty())
      return kNoPointIndex;

    const IRECT plotBounds = GetPlotBounds();
    const double maxDistanceSquared = static_cast<double>(kPointRadiusPx + 3.f) * static_cast<double>(kPointRadiusPx + 3.f);
    int hitPointIndex = kNoPointIndex;
    double closestDistanceSquared = std::numeric_limits<double>::infinity();

    for(std::size_t i = 0; i < mCurve.GetPoints().size(); ++i)
    {
      const auto& point = mCurve.GetPoints()[i];
      const double dx = static_cast<double>(XFromFrequency(point.frequencyHz, plotBounds) - x);
      const double dy = static_cast<double>(YFromDb(point.gainDb, plotBounds) - y);
      const double distanceSquared = (dx * dx) + (dy * dy);
      if(distanceSquared <= maxDistanceSquared && distanceSquared < closestDistanceSquared)
      {
        closestDistanceSquared = distanceSquared;
        hitPointIndex = static_cast<int>(i);
      }
    }

    return hitPointIndex;
  }

  void NotifyCurveChanged()
  {
    if(mOnCurveChanged)
      mOnCurveChanged(mCurve);
    SetDirty(false);
  }

  EqCurve mCurve{};
  EqCurve mRestoreCurve{};
  OnCurveChangedFunc mOnCurveChanged{};
  int mDraggedPointIndex{kNoPointIndex};
  bool mHasRestoreState{false};
  int mRestoreMidiNote{kNoRestoreMidiNote};
};
} // namespace plugin_ui

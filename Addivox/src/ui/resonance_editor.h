#pragma once

#include "../settings/resonance.h"
#include "curve_editor.h"

#include <functional>
#include <utility>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

// Edits the Resonance tab's zero-breath and full-breath curves. The active curve (selected by the Zero/Full toggle) is drawn on top and edited; the other is
// drawn faint, along with previews of the curve at 10%, 20%, ..., 90% breath. Adding or removing a point does the same to its pair on the other curve.
class ResonanceEditorControl final : public CurveEditorControl {
public:
  using Breath = ResonanceCurves::Breath;
  using PointList = ResonanceCurves::PointList;
  using OnResonanceChangedFunc = std::function<void(const ResonanceSettings&)>;

  explicit ResonanceEditorControl(const IRECT& bounds) : CurveEditorControl(bounds) {}

  static IColor BreathColor(Breath breath) { return breath == Breath::zero ? colour::editor::kZeroBreathCurve : colour::editor::kFullBreathCurve; }

  // Between key notes, the editor shows the same blend of the neighbouring key notes' setups that the synth uses.
  void SetBlendedResonance(const ResonanceSettings& lower, const ResonanceSettings& upper, double t) {
    mResonance = lower;
    mUpperResonance = upper;
    mBlendT = t;
    if (mDraggedPointIndex >= static_cast<int>(ActivePoints().size())) mDraggedPointIndex = kNoPointIndex;
    SetDirty(false);
  }

  void SetActiveBreath(Breath breath) {
    mActiveBreath = breath;
    mDraggedPointIndex = kNoPointIndex;
    SetDirty(false);
  }

  void SetEditable(bool editable) {
    if (!editable) mDraggedPointIndex = kNoPointIndex;
    if (IsEditable() == editable) return;
    SetDisabled(!editable);
    SetDirty(false);
  }

  bool IsEditable() const { return !IsDisabled(); }

  void SetOnResonanceChanged(OnResonanceChangedFunc func) { mOnResonanceChanged = std::move(func); }

  void Draw(IGraphics& g) override {
    constexpr float kInactiveOpacity = 0.35f;
    const IRECT plotBounds = GetPlotBounds();

    DrawBackgroundAndGrid(g, plotBounds);
    DrawMorphPreviews(g, plotBounds);
    DrawBreathCurve(g, plotBounds, OtherBreath(), BreathColor(OtherBreath()).WithOpacity(kInactiveOpacity), false);
    DrawBreathCurve(g, plotBounds, mActiveBreath, BreathColor(mActiveBreath), true);
    if (IsEditable()) {
      DrawPoints(g, plotBounds, OtherPoints(), BreathColor(OtherBreath()).WithOpacity(kInactiveOpacity),
                 colour::visualizer::kHarmonicCore.WithOpacity(kInactiveOpacity));
      DrawPoints(g, plotBounds, ActivePoints(), BreathColor(mActiveBreath), colour::visualizer::kHarmonicCore, mDraggedPointIndex);
    }
    DrawAxisLabels(g, plotBounds);
    if (mDraggedPointIndex != kNoPointIndex) DrawValueBubble(g, plotBounds, ActivePoints()[static_cast<std::size_t>(mDraggedPointIndex)]);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override {
    IControl::OnMouseDown(x, y, mod);
    mDraggedPointIndex = IsEditable() ? HitTestPoint(ActivePoints(), x, y) : kNoPointIndex;
    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override {
    if (mDraggedPointIndex == kNoPointIndex) return;

    mDraggedPointIndex = static_cast<int>(mResonance.curves.MovePoint(mActiveBreath, static_cast<std::size_t>(mDraggedPointIndex), PointAt(x, y)));
    NotifyResonanceChanged();
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override {
    IControl::OnMouseUp(x, y, mod);
    mDraggedPointIndex = kNoPointIndex;
    SetDirty(false);
  }

private:
  void TogglePointAt(float x, float y) override {
    if (!IsEditable()) return;

    if (const int hitIndex = HitTestPoint(ActivePoints(), x, y); hitIndex != kNoPointIndex) mResonance.curves.RemovePoint(static_cast<std::size_t>(hitIndex));
    else
      mResonance.curves.AddPoint(mActiveBreath, PointAt(x, y));
    mDraggedPointIndex = kNoPointIndex;
    NotifyResonanceChanged();
  }

  EqPoint PointAt(float x, float y) const {
    const IRECT plotBounds = GetPlotBounds();
    return {FrequencyFromX(x, plotBounds), GainDbFromY(y, plotBounds)};
  }

  // Edits only happen on a key note, where no blending applies.
  void NotifyResonanceChanged() {
    mUpperResonance = mResonance;
    if (mOnResonanceChanged) mOnResonanceChanged(mResonance);
    SetDirty(false);
  }

  // Breath Response is blended between key notes before shaping, as in the synth.
  void DrawMorphPreviews(IGraphics& g, const IRECT& plotBounds) {
    constexpr int kNumSteps = 10;
    constexpr float kPreviewOpacity = 0.25f;
    const double breathResponse = mResonance.breathResponse + (mBlendT * (mUpperResonance.breathResponse - mResonance.breathResponse));
    for (int step = 1; step < kNumSteps; ++step) {
      const double breath = static_cast<double>(step) / kNumSteps;
      const double shapedBreath = ResonanceSettings::ShapeBreath(breath, breathResponse);
      mResonance.curves.Morph(shapedBreath, mLowerMorph);
      mUpperResonance.curves.Morph(shapedBreath, mUpperMorph);
      const IColor color = IColor::LinearInterpolateBetween(BreathColor(Breath::zero), BreathColor(Breath::full), static_cast<float>(breath));
      DrawCurve(
          g, plotBounds,
          [this](double frequencyHz) {
            return EqCurve::InterpolateDb(EqCurve::EvaluateSplineDb(mLowerMorph, frequencyHz), EqCurve::EvaluateSplineDb(mUpperMorph, frequencyHz), mBlendT);
          },
          color.WithOpacity(kPreviewOpacity), false);
    }
  }

  void DrawBreathCurve(IGraphics& g, const IRECT& plotBounds, Breath breath, const IColor& color, bool bold) const {
    DrawCurve(
        g, plotBounds,
        [&](double frequencyHz) {
          return EqCurve::InterpolateDb(mResonance.curves.EvaluateDb(breath, frequencyHz), mUpperResonance.curves.EvaluateDb(breath, frequencyHz), mBlendT);
        },
        color, bold);
  }

  Breath OtherBreath() const { return ResonanceCurves::Other(mActiveBreath); }
  const PointList& ActivePoints() const { return mResonance.curves.GetPoints(mActiveBreath); }
  const PointList& OtherPoints() const { return mResonance.curves.GetPoints(OtherBreath()); }

  ResonanceSettings mResonance{};
  ResonanceSettings mUpperResonance{};
  double mBlendT{0.0};
  Breath mActiveBreath{Breath::zero};
  int mDraggedPointIndex{kNoPointIndex};
  MonotoneSpline mLowerMorph{}; // Scratch space for drawing the morph previews
  MonotoneSpline mUpperMorph{};
  OnResonanceChangedFunc mOnResonanceChanged{};
};
} // namespace plugin_ui

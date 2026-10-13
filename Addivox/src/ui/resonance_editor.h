#pragma once

#include "../settings/resonance.h"
#include "curve_editor.h"

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

// Draws the Resonance tab's zero-breath and full-breath curves. The active curve (selected by the Zero/Full toggle) is drawn on top, with the other faint.
class ResonanceEditorControl final : public CurveEditorControl {
public:
  using Breath = ResonanceCurves::Breath;

  explicit ResonanceEditorControl(const IRECT& bounds) : CurveEditorControl(bounds) {}

  static IColor BreathColor(Breath breath) { return breath == Breath::zero ? colour::editor::kZeroBreathCurve : colour::editor::kFullBreathCurve; }

  // Between key notes, the editor shows the same visual-y blend of the neighbouring curves that the synth uses.
  void SetBlendedCurves(const ResonanceCurves& lower, const ResonanceCurves& upper, double t) {
    mCurves = lower;
    mUpperCurves = upper;
    mBlendT = t;
    SetDirty(false);
  }

  void SetActiveBreath(Breath breath) {
    mActiveBreath = breath;
    SetDirty(false);
  }

  void SetEditable(bool editable) {
    if (IsEditable() == editable) return;
    SetDisabled(!editable);
    SetDirty(false);
  }

  bool IsEditable() const { return !IsDisabled(); }

  void Draw(IGraphics& g) override {
    constexpr float kInactiveOpacity = 0.35f;
    const Breath inactiveBreath = mActiveBreath == Breath::zero ? Breath::full : Breath::zero;
    const IRECT plotBounds = GetPlotBounds();

    DrawBackgroundAndGrid(g, plotBounds);
    DrawBreathCurve(g, plotBounds, inactiveBreath, BreathColor(inactiveBreath).WithOpacity(kInactiveOpacity), false);
    DrawBreathCurve(g, plotBounds, mActiveBreath, BreathColor(mActiveBreath), true);
    if (IsEditable()) {
      DrawPoints(g, plotBounds, mCurves.GetPoints(inactiveBreath), BreathColor(inactiveBreath).WithOpacity(kInactiveOpacity),
                 colour::visualizer::kHarmonicCore.WithOpacity(kInactiveOpacity));
      DrawPoints(g, plotBounds, mCurves.GetPoints(mActiveBreath), BreathColor(mActiveBreath), colour::visualizer::kHarmonicCore);
    }
    DrawAxisLabels(g, plotBounds);
  }

private:
  void DrawBreathCurve(IGraphics& g, const IRECT& plotBounds, Breath breath, const IColor& color, bool glow) const {
    DrawCurve(
        g, plotBounds,
        [&](double frequencyHz) {
          return EqCurve::InterpolateDb(mCurves.EvaluateDb(breath, frequencyHz), mUpperCurves.EvaluateDb(breath, frequencyHz), mBlendT);
        },
        color, kCurveThicknessPx, glow ? color.WithOpacity(kGlowOpacity) : COLOR_TRANSPARENT);
  }

  static constexpr float kGlowOpacity = 0.3f;

  ResonanceCurves mCurves{};
  ResonanceCurves mUpperCurves{};
  double mBlendT{0.0};
  Breath mActiveBreath{Breath::zero};
};
} // namespace plugin_ui

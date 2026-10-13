#pragma once

#include "curve_editor.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

class EqEditorControl final : public CurveEditorControl {
public:
  using OnCurveChangedFunc = std::function<void(const EqCurve&)>;

  explicit EqEditorControl(const IRECT& bounds) : CurveEditorControl(bounds) {}

  void SetCurve(const EqCurve& curve) { SetBlendedCurve(curve, curve, 0.0); }

  // Between key notes, the editor shows the same visual-y blend of the neighbouring curves that the synth uses.
  void SetBlendedCurve(const EqCurve& lower, const EqCurve& upper, double t) {
    mCurve = lower;
    mUpperCurve = upper;
    mBlendT = t;
    SyncDraggedPointIndexFromDraggedPoint();
    SetDirty(false);
  }

  void SetEditable(bool editable) {
    const bool changed = IsEditable() != editable;
    if (changed) SetDisabled(!editable);
    if (!editable) ClearDraggedPointSelection();
    if (changed) SetDirty(false);
  }

  bool IsEditable() const { return !IsDisabled(); }

  void SetOnCurveChanged(OnCurveChangedFunc func) { mOnCurveChanged = std::move(func); }

  void CaptureRestoreState(int midiNote) {
    mRestoreCurve = mCurve;
    mHasRestoreState = true;
    mRestoreMidiNote = midiNote;
  }

  void ClearRestoreState() {
    mHasRestoreState = false;
    mRestoreMidiNote = kNoRestoreMidiNote;
    mRestoreCurve = EqCurve{};
  }

  bool HasRestoreStateForMidiNote(int midiNote) const { return mHasRestoreState && (mRestoreMidiNote == midiNote); }

  const EqCurve& GetRestoreState() const { return mRestoreCurve; }

  void Draw(IGraphics& g) override {
    const IRECT plotBounds = GetPlotBounds();
    DrawBackgroundAndGrid(g, plotBounds);
    DrawCurve(
        g, plotBounds,
        [this](double frequencyHz) { return EqCurve::Lerp(mCurve.EvaluateVisualY(frequencyHz), mUpperCurve.EvaluateVisualY(frequencyHz), mBlendT); },
        colour::ui::kAccentPrimary, true);
    if (IsEditable()) DrawPoints(g, plotBounds, mCurve.GetPoints(), colour::ui::kAccentPrimary, colour::visualizer::kHarmonicCore, mDraggedPointIndex);
    DrawAxisLabels(g, plotBounds);
    if (IsEditable() && mDraggedPointIndex >= 0 && static_cast<std::size_t>(mDraggedPointIndex) < mCurve.GetPoints().size())
      DrawValueBubble(g, plotBounds, mCurve.GetPoints()[static_cast<std::size_t>(mDraggedPointIndex)]);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override {
    IControl::OnMouseDown(x, y, mod);

    if (!IsEditable()) return;

    mDraggedPointIndex = HitTestPoint(mCurve.GetPoints(), x, y);
    if (mDraggedPointIndex != kNoPointIndex) {
      mDraggedPoint = mCurve.GetPoints()[static_cast<std::size_t>(mDraggedPointIndex)];
      mHasDraggedPoint = true;
    } else
      ClearDraggedPointSelection();

    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override {
    if (!IsEditable() || mDraggedPointIndex == kNoPointIndex) return;

    auto points = mCurve.GetPoints();
    if (mDraggedPointIndex < 0 || static_cast<std::size_t>(mDraggedPointIndex) >= points.size()) return;

    const IRECT plotBounds = GetPlotBounds();
    const bool draggingRight = dX >= 0.f;
    const EqPoint updatedPoint{ResolveDraggedPointFrequency(points, mDraggedPointIndex, FrequencyFromX(x, plotBounds), draggingRight, plotBounds),
                               GainDbFromY(y, plotBounds)};
    points[static_cast<std::size_t>(mDraggedPointIndex)] = updatedPoint;
    mCurve.SetPoints(std::move(points));
    mDraggedPoint = updatedPoint;
    mHasDraggedPoint = true;
    SyncDraggedPointIndexFromDraggedPoint();
    NotifyCurveChanged();
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override {
    IControl::OnMouseUp(x, y, mod);
    ClearDraggedPointSelection();
    SetDirty(false);
  }

private:
  void TogglePointAt(float x, float y) override {
    if (!IsEditable()) return;

    auto points = mCurve.GetPoints();
    const int hitPointIndex = HitTestPoint(mCurve.GetPoints(), x, y);
    if (hitPointIndex >= 0 && static_cast<std::size_t>(hitPointIndex) < points.size()) {
      points.erase(points.begin() + hitPointIndex);
    } else {
      const IRECT plotBounds = GetPlotBounds();
      points.push_back({FrequencyFromX(x, plotBounds), GainDbFromY(y, plotBounds)});
    }
    mCurve.SetPoints(std::move(points));
    ClearDraggedPointSelection();
    NotifyCurveChanged();
  }

  static constexpr int kNoRestoreMidiNote = std::numeric_limits<int>::min();
  static constexpr double kDraggedPointFrequencyEpsilonHz = 1.0e-6;
  static constexpr double kDraggedPointGainEpsilonDb = 1.0e-6;

  int FindPointIndexMatchingDraggedPoint() const {
    if (!mHasDraggedPoint) return kNoPointIndex;

    for (std::size_t i = 0; i < mCurve.GetPoints().size(); ++i) {
      const auto& point = mCurve.GetPoints()[i];
      if (std::abs(point.frequencyHz - mDraggedPoint.frequencyHz) <= kDraggedPointFrequencyEpsilonHz &&
          std::abs(point.gainDb - mDraggedPoint.gainDb) <= kDraggedPointGainEpsilonDb) {
        return static_cast<int>(i);
      }
    }

    return kNoPointIndex;
  }

  double ResolveDraggedPointFrequency(const EqCurve::PointList& points, int draggedPointIndex, double candidateFrequencyHz, bool draggingRight,
                                      const IRECT& plotBounds) const {
    if (draggedPointIndex < 0 || static_cast<std::size_t>(draggedPointIndex) >= points.size()) return candidateFrequencyHz;

    auto collidesWithOtherPoint = [&](double frequencyHz) {
      for (std::size_t i = 0; i < points.size(); ++i) {
        if (static_cast<int>(i) == draggedPointIndex) continue;
        if (std::abs(points[i].frequencyHz - frequencyHz) <= kDraggedPointFrequencyEpsilonHz) return true;
      }

      return false;
    };

    double resolvedFrequencyHz = std::clamp(candidateFrequencyHz, EqCurve::kMinFrequencyHz, EqCurve::kMaxFrequencyHz);
    if (!collidesWithOtherPoint(resolvedFrequencyHz)) return resolvedFrequencyHz;

    const double normalizedStep = (plotBounds.W() <= 1.f) ? 1.0 : (1.0 / static_cast<double>(plotBounds.W()));

    auto offsetFrequency = [&](double frequencyHz, bool moveRight) {
      const double normalizedX = NormalizedXFromFrequency(frequencyHz);
      const double offsetNormalizedX = std::clamp(normalizedX + (moveRight ? normalizedStep : -normalizedStep), 0.0, 1.0);
      return FrequencyFromNormalizedX(static_cast<float>(offsetNormalizedX));
    };

    for (std::size_t attempt = 0; attempt < points.size(); ++attempt) {
      const double preferredFrequencyHz = offsetFrequency(resolvedFrequencyHz, draggingRight);
      if (std::abs(preferredFrequencyHz - resolvedFrequencyHz) > kDraggedPointFrequencyEpsilonHz) {
        resolvedFrequencyHz = preferredFrequencyHz;
        if (!collidesWithOtherPoint(resolvedFrequencyHz)) return resolvedFrequencyHz;
      }

      const double fallbackFrequencyHz = offsetFrequency(resolvedFrequencyHz, !draggingRight);
      if (std::abs(fallbackFrequencyHz - resolvedFrequencyHz) <= kDraggedPointFrequencyEpsilonHz) break;

      resolvedFrequencyHz = fallbackFrequencyHz;
      if (!collidesWithOtherPoint(resolvedFrequencyHz)) return resolvedFrequencyHz;
    }

    return resolvedFrequencyHz;
  }

  void SyncDraggedPointIndexFromDraggedPoint() {
    mDraggedPointIndex = FindPointIndexMatchingDraggedPoint();
    if (mDraggedPointIndex == kNoPointIndex) ClearDraggedPointSelection();
  }

  void ClearDraggedPointSelection() {
    mDraggedPointIndex = kNoPointIndex;
    mDraggedPoint = EqPoint{};
    mHasDraggedPoint = false;
  }

  void NotifyCurveChanged() {
    if (mOnCurveChanged) mOnCurveChanged(mCurve);
    SetDirty(false);
  }

  EqCurve mCurve{};
  EqCurve mUpperCurve{};
  double mBlendT{0.0};
  EqCurve mRestoreCurve{};
  OnCurveChangedFunc mOnCurveChanged{};
  EqPoint mDraggedPoint{};
  int mDraggedPointIndex{kNoPointIndex};
  bool mHasDraggedPoint{false};
  bool mHasRestoreState{false};
  int mRestoreMidiNote{kNoRestoreMidiNote};
};
} // namespace plugin_ui

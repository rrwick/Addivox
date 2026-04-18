#pragma once

#include "IControls.h"
#include "colour.h"
#include "editor_state.h"
#include "transformations.h"
#include "../settings/oscillator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

class OscillatorSliderControl final : public IVMultiSliderControl<SimplePreset::kNumOscillators>
{
public:
  using Base = IVMultiSliderControl<SimplePreset::kNumOscillators>;
  using OnOscillatorValueChangedFunc = std::function<void(int oscillatorIndex, double value)>;
  using IsOscillatorEditableFunc = std::function<bool(int oscillatorIndex)>;
  using GetOscillatorEditModeFunc = std::function<EditorOscillatorEditMode()>;
  using ControlState = std::array<double, SimplePreset::kNumOscillators>;
  using RestoreState = std::array<double, SimplePreset::kNumOscillators>;

  enum class ValueTransform
  {
    Linear,
    SquareRoot,
    PseudoLog
  };

  struct ValueRange
  {
    double min{0.0};
    double max{1.0};
  };

  struct Config
  {
    ValueRange range{};
    ValueTransform transform{ValueTransform::Linear};
  };

  explicit OscillatorSliderControl(const IRECT& bounds,
                                   const char* label = "",
                                   const IVStyle& style = DEFAULT_STYLE,
                                   EDirection direction = EDirection::Vertical)
  : Base(bounds, label, style, 0, direction)
  {
    SetOnNewValueFunc([this](int oscillatorIndex, double normalizedValue) {
      HandleSliderValueChanged(oscillatorIndex, normalizedValue);
    });
    ApplyBaseValue();
  }

  void SetConfig(const Config& config)
  {
    RestoreState oscillatorValues{};
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
      oscillatorValues[static_cast<std::size_t>(oscillatorIndex)] = GetOscillatorValue(oscillatorIndex);

    mConfig = config;
    if(!std::isfinite(mConfig.range.min) || !std::isfinite(mConfig.range.max) || mConfig.range.max <= mConfig.range.min)
      mConfig.range = ValueRange{};

    ApplyBaseValue();
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
      SetOscillatorValue(oscillatorIndex, oscillatorValues[static_cast<std::size_t>(oscillatorIndex)]);

    SetDirty(false);
  }

  const Config& GetConfig() const
  {
    return mConfig;
  }

  void SetOnOscillatorValueChanged(OnOscillatorValueChangedFunc func)
  {
    mOnOscillatorValueChanged = std::move(func);
  }

  void SetOscillatorValue(int oscillatorIndex, double value)
  {
    SetValue(ToControlValueFromRangeValue(value), oscillatorIndex);
  }

  double GetOscillatorValue(int oscillatorIndex) const
  {
    return FromControlValueToRangeValue(GetValue(oscillatorIndex));
  }

  void SetEditable(bool editable)
  {
    if(IsEditable() == editable)
      return;

    SetDisabled(!editable);
  }

  void SetOscillatorEditableFunc(IsOscillatorEditableFunc func)
  {
    mIsOscillatorEditable = std::move(func);
  }

  void SetOscillatorEditModeFunc(GetOscillatorEditModeFunc func)
  {
    mGetOscillatorEditMode = std::move(func);
  }

  void SetVisibleOscillatorRange(int minOscillatorOneBased, int maxOscillatorOneBased)
  {
    int minOscillator = std::clamp(minOscillatorOneBased - 1, 0, SimplePreset::kNumOscillators - 1);
    int maxOscillator = std::clamp(maxOscillatorOneBased - 1, 0, SimplePreset::kNumOscillators - 1);
    if(maxOscillator < minOscillator)
      maxOscillator = minOscillator;

    if(mVisibleOscillatorMin == minOscillator && mVisibleOscillatorMax == maxOscillator)
      return;

    mVisibleOscillatorMin = minOscillator;
    mVisibleOscillatorMax = maxOscillator;
    OnResize();
  }

  bool IsEditable() const
  {
    return !IsDisabled();
  }

  void CaptureRestoreState(int midiNote)
  {
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
      mRestoreState[static_cast<std::size_t>(oscillatorIndex)] = GetOscillatorValue(oscillatorIndex);

    mHasRestoreState = true;
    mRestoreMidiNote = midiNote;
  }

  void ClearRestoreState()
  {
    mHasRestoreState = false;
    mRestoreMidiNote = kNoRestoreMidiNote;
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

  const RestoreState& GetRestoreState() const
  {
    return mRestoreState;
  }

  void OnResize() override
  {
    Base::OnResize();

    const float readoutBandHeight = GetReadoutBandHeight();
    if(readoutBandHeight <= 0.f)
      return;

    IRECT plotBounds = mWidgetBounds.GetReducedFromTop(readoutBandHeight).GetReducedFromBottom(readoutBandHeight);
    mWidgetBounds = plotBounds.GetReducedFromLeft(15.f).GetReducedFromRight(15.f);
    MakeTrackRects(mWidgetBounds);
    MakeStepRects(mWidgetBounds, mNSteps);
    SetDirty(false);
  }

  void OnRescale() override
  {
    Base::OnRescale();
    OnResize();
  }

  void DrawWidget(IGraphics& g) override
  {
    Base::DrawWidget(g);

    const int oscillatorIndex = GetReadoutOscillatorIndex();
    if(oscillatorIndex < 0)
      return;

    DrawOscillatorReadout(g, oscillatorIndex);
  }

  void DrawBackground(IGraphics& g, const IRECT& r) override
  {
    g.FillRect(GetColor(kBG), r, &mBlend);

    if(mBaseValue <= 0.)
      return;

    const float scale = GetBackingPixelScale();
    if(mDirection == EDirection::Horizontal)
    {
      const float baseX = RoundToPixelBoundary(r.L + (r.W() * static_cast<float>(mBaseValue)), scale);
      g.DrawVerticalLine(GetColor(kSH), baseX, r.T, r.B, &mBlend);
    }
    else
    {
      const float baseY = RoundToPixelBoundary(r.B - (r.H() * static_cast<float>(mBaseValue)), scale);
      g.DrawHorizontalLine(GetColor(kSH), baseY, r.L, r.R, &mBlend);
    }
  }

  void DrawTrackHandle(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    const IColor fillColor = GetDisplayedBarColor(r, chIdx, aboveBaseValue);

    if(UsesBipolarRange())
    {
      DrawBipolarBarFill(g, r, aboveBaseValue, fillColor);
      return;
    }

    const IRECT alignedRect = GetAlignedFillRect(r, aboveBaseValue);
    if(alignedRect.W() <= 0.f || alignedRect.H() <= 0.f)
      return;

    DrawBarFill(g, r, alignedRect, aboveBaseValue, fillColor);
  }

  void DrawPeak(IGraphics&, const IRECT&, int, bool) override
  {
    // Remove the inherited contrasting cap for a cleaner look.
  }

  void SnapToMouse(float x,
                   float y,
                   EDirection direction,
                   const IRECT& bounds,
                   int valIdx = -1,
                   double minClip = 0.,
                   double maxClip = 1.) override
  {
    const bool allowOutsideTrackSelection = GetOscillatorEditMode() == EditorOscillatorEditMode::DrawLine;
    const auto editPoint = GetMouseEditPoint(x, y, direction, bounds, allowOutsideTrackSelection);
    const int sliderHit = editPoint.sliderHit;
    const double controlValue = editPoint.controlValue;
    bool changedValue = false;

    switch(GetOscillatorEditMode())
    {
      case EditorOscillatorEditMode::Nudge:
        changedValue = HandleNudgeDrag(sliderHit, controlValue);
        break;
      case EditorOscillatorEditMode::Smooth:
        changedValue = HandleSmoothDrag(sliderHit, controlValue);
        break;
      case EditorOscillatorEditMode::DrawLine:
        changedValue = HandleDrawLineDrag(sliderHit, controlValue);
        break;
      case EditorOscillatorEditMode::Set:
      default:
        changedValue = HandleSetDrag(sliderHit, controlValue);
        break;
    }

    SetDirty(changedValue);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ResetStepwiseDragState();
    ResetDrawLineState();

    if(GetOscillatorEditMode() == EditorOscillatorEditMode::DrawLine)
      BeginDrawLineDrag(GetMouseEditPoint(x, y, mDirection, mWidgetBounds));

    Base::OnMouseDown(x, y, mod);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    if(GetOscillatorEditMode() == EditorOscillatorEditMode::DrawLine && mHasDrawLineStartPoint)
      SnapToMouse(x, y, mDirection, mWidgetBounds);

    Base::OnMouseUp(x, y, mod);
    ResetStepwiseDragState();
    ResetDrawLineState();
  }

private:
  static constexpr float kReadoutMaxBandHeight = 16.f;
  static constexpr float kReadoutMinBandHeight = 8.f;
  static constexpr float kReadoutTextWidth = 72.f;
  static constexpr double kNudgeControlStep = 0.005;
  static constexpr double kValueComparisonTolerance = 1.0e-9;
  static constexpr std::array<double, 7> kSmoothControlKernel{{0.01, 0.05, 0.10, 0.68, 0.10, 0.05, 0.01}};

  struct MouseEditPoint
  {
    int sliderHit{-1};
    double controlValue{0.0};
  };

  static double Clamp01(double value)
  {
    return std::clamp(value, 0.0, 1.0);
  }

  static double ClampSignedUnit(double value)
  {
    return std::clamp(value, -1.0, 1.0);
  }

  static float FloorToPixelBoundary(float value, float scale)
  {
    return static_cast<float>(std::floor(static_cast<double>(value) * static_cast<double>(scale))
                              / static_cast<double>(scale));
  }

  static float CeilToPixelBoundary(float value, float scale)
  {
    return static_cast<float>(std::ceil(static_cast<double>(value) * static_cast<double>(scale))
                              / static_cast<double>(scale));
  }

  static float RoundToPixelBoundary(float value, float scale)
  {
    return static_cast<float>(std::round(static_cast<double>(value) * static_cast<double>(scale))
                              / static_cast<double>(scale));
  }

  float GetBackingPixelScale() const
  {
    if(const IGraphics* ui = GetUI())
      return std::max(1.f, ui->GetTotalScale());

    return 1.f;
  }

  static IColor ScaleColorOpacity(const IColor& color, float opacity)
  {
    IColor scaled = color;
    scaled.A = static_cast<int>(std::round(static_cast<float>(color.A) * std::clamp(opacity, 0.f, 1.f)));
    return scaled;
  }

  static IColor LightenColor(const IColor& color, float amount)
  {
    return colour::visualizer::LerpColor(color, COLOR_WHITE, std::clamp(amount, 0.f, 1.f));
  }

  float GetFillLength(const IRECT& rect) const
  {
    return mDirection == EDirection::Horizontal ? rect.W() : rect.H();
  }

  float GetTrackLength(const IRECT& rect) const
  {
    return mDirection == EDirection::Horizontal ? rect.W() : rect.H();
  }

  IColor GetBaseBarColor(const IRECT& fillRect, int chIdx, bool aboveBaseValue) const
  {
    const IRECT& trackBounds = mTrackBounds.Get()[chIdx];
    if(trackBounds.Empty())
      return colour::visualizer::GradientColor(0.f);

    const float currentLength = std::max(0.f, GetFillLength(fillRect));

    if(UsesBipolarRange())
    {
      const float basePosition = mDirection == EDirection::Horizontal
        ? (trackBounds.L + (trackBounds.W() * static_cast<float>(mBaseValue)))
        : (trackBounds.B - (trackBounds.H() * static_cast<float>(mBaseValue)));
      const float maxLength = mDirection == EDirection::Horizontal
        ? (aboveBaseValue ? (trackBounds.R - basePosition) : (basePosition - trackBounds.L))
        : (aboveBaseValue ? (basePosition - trackBounds.T) : (trackBounds.B - basePosition));
      const float normalizedLength = (maxLength > 0.f)
        ? std::clamp(currentLength / maxLength, 0.f, 1.f)
        : 0.f;
      return colour::visualizer::GradientColor(0.5f + ((aboveBaseValue ? 1.f : -1.f) * normalizedLength * 0.5f));
    }

    const float trackLength = GetTrackLength(trackBounds);
    const float normalizedLength = (trackLength > 0.f)
      ? std::clamp(currentLength / trackLength, 0.f, 1.f)
      : 0.f;
    return colour::visualizer::GradientColor(normalizedLength);
  }

  IColor GetDisplayedBarColor(const IRECT& fillRect, int chIdx, bool aboveBaseValue) const
  {
    const IColor baseColor = GetBaseBarColor(fillRect, chIdx, aboveBaseValue);

    float lightenAmount = 0.f;
    if(chIdx == mHighlightedTrack)
      lightenAmount += 0.35f;
    if(chIdx == mMouseOverTrack)
      lightenAmount += 0.25f;

    return lightenAmount > 0.f ? LightenColor(baseColor, std::min(lightenAmount, 0.6f)) : baseColor;
  }

  static std::pair<float, float> GetPixelAlignedLengthParts(float length, float scale)
  {
    const double scaledLength = std::max(0.0, static_cast<double>(length) * static_cast<double>(scale));
    double wholePixels = 0.0;
    double fractional = std::modf(scaledLength, &wholePixels);

    if(fractional <= 1.0e-6)
      fractional = 0.0;
    else if((1.0 - fractional) <= 1.0e-6)
    {
      wholePixels += 1.0;
      fractional = 0.0;
    }

    return {
      static_cast<float>(wholePixels / static_cast<double>(scale)),
      static_cast<float>(fractional)
    };
  }

  static float GetPositiveEdgeCoverage(float edge, float scale)
  {
    const double scaledEdge = static_cast<double>(edge) * static_cast<double>(scale);
    const double fractional = scaledEdge - std::floor(scaledEdge);

    if(fractional <= 1.0e-6 || fractional >= 1.0 - 1.0e-6)
      return 0.f;

    return static_cast<float>(fractional);
  }

  static float GetNegativeEdgeCoverage(float edge, float scale)
  {
    const float coverage = GetPositiveEdgeCoverage(edge, scale);
    if(coverage <= 0.f)
      return 0.f;

    return 1.f - coverage;
  }

  float GetTipCoverage(const IRECT& r, bool aboveBaseValue, float scale) const
  {
    if(r.W() <= 0.f || r.H() <= 0.f)
      return 0.f;

    if(mDirection == EDirection::Horizontal)
      return aboveBaseValue ? GetPositiveEdgeCoverage(r.R, scale) : GetNegativeEdgeCoverage(r.L, scale);

    return aboveBaseValue ? GetNegativeEdgeCoverage(r.T, scale) : GetPositiveEdgeCoverage(r.B, scale);
  }

  IRECT GetBipolarCoreRect(const IRECT& r, bool aboveBaseValue, float scale) const
  {
    const float thickness = 2.f / scale;
    const float halfThickness = thickness * 0.5f;

    if(mDirection == EDirection::Horizontal)
    {
      const float baseX = aboveBaseValue ? r.L : r.R;
      const float left = RoundToPixelBoundary(baseX - halfThickness, scale);
      return IRECT(left, r.T, left + thickness, r.B);
    }

    const float baseY = aboveBaseValue ? r.B : r.T;
    const float top = RoundToPixelBoundary(baseY - halfThickness, scale);
    return IRECT(r.L, top, r.R, top + thickness);
  }

  IRECT ExtendRectOutward(const IRECT& anchorRect, float length, bool aboveBaseValue) const
  {
    if(length <= 0.f)
      return IRECT();

    if(mDirection == EDirection::Horizontal)
    {
      if(aboveBaseValue)
        return IRECT(anchorRect.R, anchorRect.T, anchorRect.R + length, anchorRect.B);

      return IRECT(anchorRect.L - length, anchorRect.T, anchorRect.L, anchorRect.B);
    }

    if(aboveBaseValue)
      return IRECT(anchorRect.L, anchorRect.T - length, anchorRect.R, anchorRect.T);

    return IRECT(anchorRect.L, anchorRect.B, anchorRect.R, anchorRect.B + length);
  }

  void DrawBipolarBarFill(IGraphics& g,
                          const IRECT& originalRect,
                          bool aboveBaseValue,
                          const IColor& color) const
  {
    const float scale = GetBackingPixelScale();
    const float tipThickness = 1.f / scale;

    // Keep a stable 2-pixel core centered on zero so exact-zero and near-zero
    // bipolar values stay visually ordered while the value grows outward.
    const IRECT coreRect = GetBipolarCoreRect(originalRect, aboveBaseValue, scale);
    g.FillRect(color, coreRect, &mBlend);

    const float extensionLength = mDirection == EDirection::Horizontal ? originalRect.W() : originalRect.H();
    if(extensionLength <= 0.f)
      return;

    const auto [solidLength, tipCoverage] = GetPixelAlignedLengthParts(extensionLength, scale);
    IRECT anchorRect = coreRect;

    if(solidLength > 0.f)
    {
      const IRECT bodyRect = ExtendRectOutward(coreRect, solidLength, aboveBaseValue);
      if(bodyRect.W() > 0.f && bodyRect.H() > 0.f)
      {
        g.FillRect(color, bodyRect, &mBlend);
        anchorRect = bodyRect;
      }
    }

    if(tipCoverage > 0.f)
    {
      const IRECT tipRect = ExtendRectOutward(anchorRect, tipThickness, aboveBaseValue);
      if(tipRect.W() > 0.f && tipRect.H() > 0.f)
        DrawPixelAlignedTip(g, tipRect, ScaleColorOpacity(color, tipCoverage));
    }
  }

  void DrawPixelAlignedTip(IGraphics& g, const IRECT& tipRect, const IColor& color) const
  {
    if(tipRect.W() <= 0.f || tipRect.H() <= 0.f)
      return;

    // Use a line rather than a tiny rectangle so the partially covered final
    // pixel row/column gets uniform alpha across the bar width.
    if(mDirection == EDirection::Horizontal)
    {
      const float x = tipRect.MW();
      g.DrawVerticalLine(color, x, tipRect.T, tipRect.B, &mBlend, tipRect.W());
    }
    else
    {
      const float y = tipRect.MH();
      g.DrawHorizontalLine(color, y, tipRect.L, tipRect.R, &mBlend, tipRect.H());
    }
  }

  void SplitAlignedTipRect(IRECT& bodyRect, IRECT& tipRect, bool aboveBaseValue, float tipThickness) const
  {
    if(mDirection == EDirection::Horizontal)
    {
      if(aboveBaseValue)
      {
        bodyRect.R = std::max(bodyRect.L, bodyRect.R - tipThickness);
        tipRect.L = std::max(tipRect.L, tipRect.R - tipThickness);
      }
      else
      {
        bodyRect.L = std::min(bodyRect.R, bodyRect.L + tipThickness);
        tipRect.R = std::min(tipRect.R, tipRect.L + tipThickness);
      }
    }
    else
    {
      if(aboveBaseValue)
      {
        bodyRect.T = std::min(bodyRect.B, bodyRect.T + tipThickness);
        tipRect.B = std::min(tipRect.B, tipRect.T + tipThickness);
      }
      else
      {
        bodyRect.B = std::max(bodyRect.T, bodyRect.B - tipThickness);
        tipRect.T = std::max(tipRect.T, tipRect.B - tipThickness);
      }
    }
  }

  void DrawBarFill(IGraphics& g,
                   const IRECT& originalRect,
                   const IRECT& alignedRect,
                   bool aboveBaseValue,
                   const IColor& color) const
  {
    const float scale = GetBackingPixelScale();
    const float tipCoverage = GetTipCoverage(originalRect, aboveBaseValue, scale);
    if(tipCoverage <= 0.f)
    {
      g.FillRect(color, alignedRect, &mBlend);
      return;
    }

    const float tipThickness = std::min(1.f / scale,
                                        mDirection == EDirection::Horizontal ? alignedRect.W() : alignedRect.H());
    IRECT bodyRect = alignedRect;
    IRECT tipRect = alignedRect;
    SplitAlignedTipRect(bodyRect, tipRect, aboveBaseValue, tipThickness);

    if(bodyRect.W() > 0.f && bodyRect.H() > 0.f)
      g.FillRect(color, bodyRect, &mBlend);

    if(tipRect.W() > 0.f && tipRect.H() > 0.f)
      DrawPixelAlignedTip(g, tipRect, ScaleColorOpacity(color, tipCoverage));
  }

  IRECT GetAlignedFillRect(const IRECT& r, bool aboveBaseValue) const
  {
    if(r.W() <= 0.f || r.H() <= 0.f)
      return IRECT();

    const float scale = GetBackingPixelScale();
    IRECT aligned = r;

    if(mDirection == EDirection::Horizontal)
    {
      if(aboveBaseValue)
      {
        aligned.L = RoundToPixelBoundary(r.L, scale);
        aligned.R = CeilToPixelBoundary(r.R, scale);
      }
      else
      {
        aligned.L = FloorToPixelBoundary(r.L, scale);
        aligned.R = RoundToPixelBoundary(r.R, scale);
      }
    }
    else
    {
      if(aboveBaseValue)
      {
        aligned.T = FloorToPixelBoundary(r.T, scale);
        aligned.B = RoundToPixelBoundary(r.B, scale);
      }
      else
      {
        aligned.T = RoundToPixelBoundary(r.T, scale);
        aligned.B = CeilToPixelBoundary(r.B, scale);
      }
    }

    return aligned;
  }

  static float GetSnappedSubdivisionEdge(float origin, float length, int index, int count, float scale)
  {
    const double scaledOrigin = static_cast<double>(origin) * static_cast<double>(scale);
    const double scaledLength = static_cast<double>(length) * static_cast<double>(scale);
    const double scaledEdge = scaledOrigin + (scaledLength * static_cast<double>(index)) / static_cast<double>(count);
    return static_cast<float>(std::round(scaledEdge) / static_cast<double>(scale));
  }

  EditorOscillatorEditMode GetOscillatorEditMode() const
  {
    return mGetOscillatorEditMode ? mGetOscillatorEditMode() : EditorOscillatorEditMode::Set;
  }

  int FindOscillatorIndexForPoint(float x, float y, EDirection direction) const
  {
    const int nVals = NVals();
    for(int oscillatorIndex = 0; oscillatorIndex < nVals; ++oscillatorIndex)
    {
      const IRECT& trackBounds = mTrackBounds.Get()[oscillatorIndex];
      if(trackBounds.Empty())
        continue;

      const bool isHit = direction == EDirection::Vertical
        ? trackBounds.ContainsEdge(x, trackBounds.MH())
        : trackBounds.ContainsEdge(trackBounds.MW(), y);
      if(isHit)
        return oscillatorIndex;
    }

    return -1;
  }

  int FindNearestVisibleOscillatorIndex(float position, EDirection direction) const
  {
    int nearestOscillatorIndex = -1;
    float nearestDistance = std::numeric_limits<float>::max();
    const int nVals = NVals();

    for(int oscillatorIndex = 0; oscillatorIndex < nVals; ++oscillatorIndex)
    {
      const IRECT& trackBounds = mTrackBounds.Get()[oscillatorIndex];
      if(trackBounds.Empty())
        continue;

      const float trackCenter = direction == EDirection::Vertical
        ? trackBounds.MW()
        : trackBounds.MH();
      const float distance = std::fabs(position - trackCenter);
      if(distance < nearestDistance)
      {
        nearestDistance = distance;
        nearestOscillatorIndex = oscillatorIndex;
      }
    }

    return nearestOscillatorIndex;
  }

  MouseEditPoint GetMouseEditPoint(float x,
                                   float y,
                                   EDirection direction,
                                   const IRECT& bounds,
                                   bool allowOutsideTrackSelection = false) const
  {
    float constrainedX = x;
    float constrainedY = y;
    bounds.Constrain(constrainedX, constrainedY);

    double value = 0.0;
    int sliderHit = -1;

    const int step = GetStepIdxForPos(constrainedX, constrainedY);

    if(direction == EDirection::Vertical)
    {
      if(step > -1)
      {
        constrainedY = mStepBounds.Get()[step].T;

        if(mStepBounds.GetSize() == 1)
          value = 1.f;
        else
          value = step * (1.f / float(mStepBounds.GetSize() - 1));
      }
      else
      {
        value = 1.f - (constrainedY - bounds.T) / bounds.H();
      }

      sliderHit = FindOscillatorIndexForPoint(allowOutsideTrackSelection ? x : constrainedX, constrainedY, direction);
      if(sliderHit < 0 && allowOutsideTrackSelection)
        sliderHit = FindNearestVisibleOscillatorIndex(x, direction);
    }
    else
    {
      if(step > -1)
      {
        constrainedX = mStepBounds.Get()[step].L;

        if(mStepBounds.GetSize() == 1)
          value = 1.f;
        else
          value = 1.f - (step * (1.f / float(mStepBounds.GetSize() - 1)));
      }
      else
      {
        value = (constrainedX - bounds.L) / bounds.W();
      }

      sliderHit = FindOscillatorIndexForPoint(constrainedX, allowOutsideTrackSelection ? y : constrainedY, direction);
      if(sliderHit < 0 && allowOutsideTrackSelection)
        sliderHit = FindNearestVisibleOscillatorIndex(y, direction);
    }

    if(!GetStepped())
      value = std::round(value / mGrain) * mGrain;

    return MouseEditPoint{sliderHit, Clamp01(value)};
  }

  void ResetStepwiseDragState()
  {
    mHasPreviousStepwiseDragPoint = false;
  }

  void ResetDrawLineState()
  {
    mHasDrawLineStartPoint = false;
    mDrawLineStartOscillator = -1;
    mDrawLineStartControlValue = 0.0;
  }

  void BeginDrawLineDrag(const MouseEditPoint& startPoint)
  {
    if(startPoint.sliderHit < 0)
      return;

    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
      mDrawLineSourceValues[static_cast<std::size_t>(oscillatorIndex)] = GetValue(oscillatorIndex);

    mDrawLineStartOscillator = startPoint.sliderHit;
    mDrawLineStartControlValue = startPoint.controlValue;
    mHasDrawLineStartPoint = true;
  }

  bool HandleSetDrag(int sliderHit, double controlValue)
  {
    bool changedValue = false;

    if(sliderHit > -1)
    {
      mMouseOverTrack = sliderHit;

      if(IsOscillatorEditable(sliderHit))
      {
        SetValue(controlValue, sliderHit);
        OnNewValue(sliderHit, GetValue(sliderHit));
        changedValue = true;

        mSliderHit = sliderHit;

        if(!GetStepped() && mPrevSliderHit != -1)
        {
          if(std::abs(mPrevSliderHit - mSliderHit) > 1)
          {
            const int lowBounds = std::min(mPrevSliderHit, mSliderHit);
            const int highBounds = std::max(mPrevSliderHit, mSliderHit);

            for(int oscillatorIndex = lowBounds; oscillatorIndex < highBounds; ++oscillatorIndex)
            {
              if(!IsOscillatorEditable(oscillatorIndex))
                continue;

              const double frac = static_cast<double>(oscillatorIndex - lowBounds)
                                / static_cast<double>(highBounds - lowBounds);
              SetValue(iplug::Lerp(GetValue(lowBounds), GetValue(highBounds), frac), oscillatorIndex);
              OnNewValue(oscillatorIndex, GetValue(oscillatorIndex));
              changedValue = true;
            }
          }
        }

        mPrevSliderHit = mSliderHit;
      }
      else
      {
        mSliderHit = -1;
      }
    }
    else
    {
      mSliderHit = -1;
    }

    return changedValue;
  }

  template <typename StepFunc>
  bool HandleStepwiseDrag(int sliderHit, double cursorControlValue, StepFunc&& applyStep)
  {
    if(sliderHit > -1)
      mMouseOverTrack = sliderHit;
    else
      mSliderHit = -1;

    if(sliderHit < 0)
      return false;

    mSliderHit = sliderHit;

    bool changedValue = false;
    const bool hasPreviousPoint = mHasPreviousStepwiseDragPoint && mPrevSliderHit >= 0;

    if(!hasPreviousPoint)
    {
      changedValue = applyStep(sliderHit, cursorControlValue);
    }
    else if(mPrevSliderHit == sliderHit)
    {
      mPreviousStepwiseCursorValue = cursorControlValue;
      mHasPreviousStepwiseDragPoint = true;
      return false;
    }
    else
    {
      const int previousSliderHit = mPrevSliderHit;
      const int lowBounds = std::min(previousSliderHit, sliderHit);
      const int highBounds = std::max(previousSliderHit, sliderHit);
      const double previousCursorValue = mPreviousStepwiseCursorValue;
      const int span = highBounds - lowBounds;

      for(int oscillatorIndex = lowBounds; oscillatorIndex <= highBounds; ++oscillatorIndex)
      {
        if(oscillatorIndex == previousSliderHit)
          continue;

        const double frac = span > 0
          ? static_cast<double>(std::abs(oscillatorIndex - previousSliderHit)) / static_cast<double>(span)
          : 1.0;
        const double interpolatedCursorValue = iplug::Lerp(previousCursorValue, cursorControlValue, frac);
        changedValue = applyStep(oscillatorIndex, interpolatedCursorValue) || changedValue;
      }
    }

    mPrevSliderHit = sliderHit;
    mPreviousStepwiseCursorValue = cursorControlValue;
    mHasPreviousStepwiseDragPoint = true;
    return changedValue;
  }

  bool HandleNudgeDrag(int sliderHit, double cursorControlValue)
  {
    return HandleStepwiseDrag(
      sliderHit,
      cursorControlValue,
      [this](int oscillatorIndex, double stepCursorControlValue) {
        return ApplyNudgeStep(oscillatorIndex, stepCursorControlValue);
      });
  }

  bool HandleSmoothDrag(int sliderHit, double cursorControlValue)
  {
    return HandleStepwiseDrag(
      sliderHit,
      cursorControlValue,
      [this](int oscillatorIndex, double) {
        return ApplySmoothStep(oscillatorIndex);
      });
  }

  bool HandleDrawLineDrag(int sliderHit, double cursorControlValue)
  {
    if(sliderHit > -1)
      mMouseOverTrack = sliderHit;
    else
      mSliderHit = -1;

    if(sliderHit < 0 || !mHasDrawLineStartPoint)
      return false;

    mSliderHit = sliderHit;
    return ApplyDrawLinePreview(sliderHit, cursorControlValue);
  }

  bool ApplyNudgeStep(int oscillatorIndex, double cursorControlValue)
  {
    if(!IsOscillatorEditable(oscillatorIndex))
      return false;

    const double currentControlValue = GetValue(oscillatorIndex);
    double nudgedControlValue = currentControlValue;

    if(cursorControlValue > currentControlValue + kValueComparisonTolerance)
      nudgedControlValue = Clamp01(currentControlValue + kNudgeControlStep);
    else if(cursorControlValue < currentControlValue - kValueComparisonTolerance)
      nudgedControlValue = Clamp01(currentControlValue - kNudgeControlStep);
    else
      return false;

    if(std::fabs(nudgedControlValue - currentControlValue) <= kValueComparisonTolerance)
      return false;

    SetValue(nudgedControlValue, oscillatorIndex);
    OnNewValue(oscillatorIndex, nudgedControlValue);
    return true;
  }

  double GetSmoothedControlValue(int oscillatorIndex) const
  {
    double weightedValue = 0.0;
    double totalWeight = 0.0;

    for(int offset = -3; offset <= 3; ++offset)
    {
      const int neighbourIndex = oscillatorIndex + offset;
      if(neighbourIndex < 0 || neighbourIndex >= SimplePreset::kNumOscillators)
        continue;
      if(!IsOscillatorEditable(neighbourIndex))
        continue;

      const double weight = kSmoothControlKernel[static_cast<std::size_t>(offset + 3)];
      weightedValue += GetValue(neighbourIndex) * weight;
      totalWeight += weight;
    }

    if(totalWeight <= 0.0)
      return GetValue(oscillatorIndex);

    return Clamp01(weightedValue / totalWeight);
  }

  bool ApplySmoothStep(int oscillatorIndex)
  {
    if(!IsOscillatorEditable(oscillatorIndex))
      return false;

    const double currentControlValue = GetValue(oscillatorIndex);
    const double smoothedControlValue = GetSmoothedControlValue(oscillatorIndex);
    if(std::fabs(smoothedControlValue - currentControlValue) <= kValueComparisonTolerance)
      return false;

    SetValue(smoothedControlValue, oscillatorIndex);
    OnNewValue(oscillatorIndex, smoothedControlValue);
    return true;
  }

  bool ApplyDrawLinePreview(int endOscillatorIndex, double endControlValue)
  {
    if(mDrawLineStartOscillator < 0)
      return false;

    ControlState desiredValues = mDrawLineSourceValues;
    const int startOscillatorIndex = mDrawLineStartOscillator;
    const double startControlValue = mDrawLineStartControlValue;

    if(startOscillatorIndex == endOscillatorIndex)
    {
      if(IsOscillatorEditable(startOscillatorIndex))
        desiredValues[static_cast<std::size_t>(startOscillatorIndex)] = Clamp01((startControlValue + endControlValue) * 0.5);
    }
    else
    {
      const int span = std::abs(endOscillatorIndex - startOscillatorIndex);
      const int lowBounds = std::min(startOscillatorIndex, endOscillatorIndex);
      const int highBounds = std::max(startOscillatorIndex, endOscillatorIndex);

      for(int oscillatorIndex = lowBounds; oscillatorIndex <= highBounds; ++oscillatorIndex)
      {
        if(!IsOscillatorEditable(oscillatorIndex))
          continue;

        const double frac = static_cast<double>(std::abs(oscillatorIndex - startOscillatorIndex)) / static_cast<double>(span);
        desiredValues[static_cast<std::size_t>(oscillatorIndex)] =
          Clamp01(iplug::Lerp(startControlValue, endControlValue, frac));
      }
    }

    return ApplyControlState(desiredValues);
  }

  bool ApplyControlState(const ControlState& desiredValues)
  {
    bool changedValue = false;

    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
    {
      const double currentControlValue = GetValue(oscillatorIndex);
      const double desiredControlValue = Clamp01(desiredValues[static_cast<std::size_t>(oscillatorIndex)]);
      if(std::fabs(desiredControlValue - currentControlValue) <= kValueComparisonTolerance)
        continue;

      SetValue(desiredControlValue, oscillatorIndex);
      OnNewValue(oscillatorIndex, desiredControlValue);
      changedValue = true;
    }

    return changedValue;
  }

  float GetReadoutBandHeight() const
  {
    return std::clamp(mRECT.H() * 0.06f, kReadoutMinBandHeight, kReadoutMaxBandHeight);
  }

  double ApplyTransform(double normalizedValue) const
  {
    const double clamped = Clamp01(normalizedValue);
    switch(mConfig.transform)
    {
      case ValueTransform::SquareRoot:
        return transformations::NormalizedSquareRoot(clamped);
      case ValueTransform::PseudoLog:
        return transformations::NormalizedExpInverse(clamped, transformations::GetGlobalPseudoLogShapeValue());
      case ValueTransform::Linear:
      default:
        return clamped;
    }
  }

  double InvertTransform(double controlValue) const
  {
    const double clamped = Clamp01(controlValue);
    switch(mConfig.transform)
    {
      case ValueTransform::SquareRoot:
        return transformations::NormalizedSquareRootInverse(clamped);
      case ValueTransform::PseudoLog:
        return transformations::NormalizedExp(clamped, transformations::GetGlobalPseudoLogShapeValue());
      case ValueTransform::Linear:
      default:
        return clamped;
    }
  }

  double ApplySignedTransform(double normalizedValue) const
  {
    const double clamped = ClampSignedUnit(normalizedValue);
    switch(mConfig.transform)
    {
      case ValueTransform::SquareRoot:
        return transformations::SignedNormalizedSquareRoot(clamped);
      case ValueTransform::PseudoLog:
        return transformations::SignedNormalizedExpInverse(clamped, transformations::GetGlobalPseudoLogShapeValue());
      case ValueTransform::Linear:
      default:
        return clamped;
    }
  }

  double InvertSignedTransform(double controlValue) const
  {
    const double clamped = ClampSignedUnit(controlValue);
    switch(mConfig.transform)
    {
      case ValueTransform::SquareRoot:
        return transformations::SignedNormalizedSquareRootInverse(clamped);
      case ValueTransform::PseudoLog:
        return transformations::SignedNormalizedExp(clamped, transformations::GetGlobalPseudoLogShapeValue());
      case ValueTransform::Linear:
      default:
        return clamped;
    }
  }

  double Normalize(double value) const
  {
    const double range = mConfig.range.max - mConfig.range.min;
    if(range <= 0.0)
      return 0.0;

    return Clamp01((value - mConfig.range.min) / range);
  }

  double Denormalize(double normalizedValue) const
  {
    const double range = mConfig.range.max - mConfig.range.min;
    return mConfig.range.min + (Clamp01(normalizedValue) * range);
  }

  bool UsesBipolarRange() const
  {
    return mConfig.range.min < 0.0 && mConfig.range.max > 0.0;
  }

  bool UsesMirroredBipolarTransform() const
  {
    if(!UsesBipolarRange())
      return false;

    const double minMagnitude = std::fabs(mConfig.range.min);
    const double maxMagnitude = std::fabs(mConfig.range.max);
    const double tolerance = std::max({1.0, minMagnitude, maxMagnitude}) * 1.0e-9;
    return std::fabs(minMagnitude - maxMagnitude) <= tolerance;
  }

  double GetMirroredBipolarMaxMagnitude() const
  {
    return std::max(std::fabs(mConfig.range.min), std::fabs(mConfig.range.max));
  }

  double ToControlValueFromRangeValue(double value) const
  {
    const double clampedValue = std::clamp(value, mConfig.range.min, mConfig.range.max);
    if(!UsesMirroredBipolarTransform())
      return ApplyTransform(Normalize(clampedValue));

    const double maxMagnitude = GetMirroredBipolarMaxMagnitude();
    if(maxMagnitude <= 0.0)
      return 0.5;

    const double normalizedValue = ClampSignedUnit(clampedValue / maxMagnitude);
    return 0.5 + (ApplySignedTransform(normalizedValue) * 0.5);
  }

  double FromControlValueToRangeValue(double controlValue) const
  {
    const double clampedControlValue = Clamp01(controlValue);
    if(!UsesMirroredBipolarTransform())
      return Denormalize(InvertTransform(clampedControlValue));

    const double maxMagnitude = GetMirroredBipolarMaxMagnitude();
    if(maxMagnitude <= 0.0)
      return 0.0;

    const double normalizedValue = InvertSignedTransform((clampedControlValue * 2.0) - 1.0);
    return std::clamp(normalizedValue * maxMagnitude, mConfig.range.min, mConfig.range.max);
  }

  void HandleSliderValueChanged(int oscillatorIndex, double normalizedValue)
  {
    if(mOnOscillatorValueChanged)
      mOnOscillatorValueChanged(oscillatorIndex, FromControlValueToRangeValue(normalizedValue));
  }

  void ApplyBaseValue()
  {
    if(mConfig.range.min < 0.0 && mConfig.range.max > 0.0)
      SetBaseValue(ToControlValueFromRangeValue(0.0));
    else
      SetBaseValue(0.0);
  }

  int GetReadoutOscillatorIndex() const
  {
    if(IsVisibleOscillatorIndex(mMouseOverTrack))
      return mMouseOverTrack;

    if(IsVisibleOscillatorIndex(mHighlightedTrack))
      return mHighlightedTrack;

    return -1;
  }

  bool IsVisibleOscillatorIndex(int oscillatorIndex) const
  {
    return oscillatorIndex >= 0
      && oscillatorIndex < NVals()
      && !mTrackBounds.Get()[oscillatorIndex].Empty();
  }

  void DrawOscillatorReadout(IGraphics& g, int oscillatorIndex) const
  {
    const IRECT& trackBounds = mTrackBounds.Get()[oscillatorIndex];
    if(trackBounds.Empty())
      return;

    const float readoutBandHeight = std::max(0.f, GetReadoutBandHeight());
    if(readoutBandHeight <= 0.f)
      return;

    const float centerX = trackBounds.MW();
    const IRECT topBand = MakeReadoutRect(IRECT(mRECT.L, mRECT.T, mRECT.R, mWidgetBounds.T), centerX);
    const IRECT bottomBand = MakeReadoutRect(IRECT(mRECT.L, mWidgetBounds.B, mRECT.R, mRECT.B), centerX);

    const WDL_String harmonicNumber = FormatHarmonicNumber(oscillatorIndex);
    const WDL_String value = FormatOscillatorValue(oscillatorIndex);

    g.DrawText(MakeReadoutText("Roboto-Black", 12.f, EAlign::Center, EVAlign::Bottom), value.Get(), topBand, &mBlend);
    g.DrawText(MakeReadoutText("Roboto-Black", 12.f, EAlign::Center, EVAlign::Top), harmonicNumber.Get(), bottomBand, &mBlend);
  }

  IRECT MakeReadoutRect(const IRECT& bandBounds, float centerX) const
  {
    const float width = std::min(kReadoutTextWidth, bandBounds.W());
    const float left = centerX - (width * 0.5f);
    return IRECT(left, bandBounds.T, left + width, bandBounds.B);
  }

  IText MakeReadoutText(const char* font, float size, EAlign align, EVAlign valign) const
  {
    return IText(size, colour::ui::kValueText, font, align, valign);
  }

  WDL_String FormatHarmonicNumber(int oscillatorIndex) const
  {
    WDL_String text;
    text.SetFormatted(8, "%d", oscillatorIndex + 1);
    return text;
  }

  WDL_String FormatOscillatorValue(int oscillatorIndex) const
  {
    const double value = GetOscillatorValue(oscillatorIndex);
    WDL_String text;
    const double absValue = std::fabs(value);
    if(absValue < 1e-12)
      text.Set("0.0");
    else if(absValue >= 1.0)
      text.SetFormatted(16, "%.1f", value);
    else
      text.SetFormatted(16, "%.*f", GetSmallValueDecimalDigits(absValue), value);

    return text;
  }

  static int GetSmallValueDecimalDigits(double absValue)
  {
    const int digits = 1 - static_cast<int>(std::floor(std::log10(absValue)));
    return std::clamp(digits, 1, 5);
  }

  bool IsOscillatorEditable(int oscillatorIndex) const
  {
    return !mIsOscillatorEditable || mIsOscillatorEditable(oscillatorIndex);
  }

  void MakeTrackRects(const IRECT& bounds) override
  {
    const int nVals = NVals();
    const int dir = static_cast<int>(mDirection);
    const float scale = GetBackingPixelScale();
    const float gapSize = 2.f / scale;

    for(int ch = 0; ch < nVals; ++ch)
      mTrackBounds.Get()[ch] = IRECT();

    const int visibleMin = std::clamp(mVisibleOscillatorMin, 0, nVals - 1);
    const int visibleMax = std::clamp(mVisibleOscillatorMax, visibleMin, nVals - 1);
    const int visibleCount = visibleMax - visibleMin + 1;

    for(int ch = visibleMin; ch <= visibleMax; ++ch)
    {
      const int visibleIndex = ch - visibleMin;
      IRECT trackBounds = bounds.SubRect(EDirection(!dir), visibleCount, visibleIndex)
                                .GetPadded(0, -mTrackPadding * static_cast<float>(dir),
                                           -mTrackPadding * static_cast<float>(!dir), -mTrackPadding);

      if(mDirection == EDirection::Horizontal)
      {
        trackBounds.T = GetSnappedSubdivisionEdge(bounds.T, bounds.H(), visibleIndex, visibleCount, scale);
        trackBounds.B = GetSnappedSubdivisionEdge(bounds.T, bounds.H(), visibleIndex + 1, visibleCount, scale);
      }
      else
      {
        trackBounds.L = GetSnappedSubdivisionEdge(bounds.L, bounds.W(), visibleIndex, visibleCount, scale);
        trackBounds.R = GetSnappedSubdivisionEdge(bounds.L, bounds.W(), visibleIndex + 1, visibleCount, scale);
      }

      // Keep a visible separator between adjacent bars after snapping columns to the pixel grid.
      if(ch < visibleMax)
      {
        if(mDirection == EDirection::Horizontal)
          trackBounds.B = std::max(trackBounds.T, trackBounds.B - gapSize);
        else
          trackBounds.R = std::max(trackBounds.L, trackBounds.R - gapSize);
      }

      mTrackBounds.Get()[ch] = trackBounds;
    }
  }

  Config mConfig{};
  RestoreState mRestoreState{};
  bool mHasRestoreState{false};
  static constexpr int kNoRestoreMidiNote = std::numeric_limits<int>::min();
  int mRestoreMidiNote{kNoRestoreMidiNote};
  int mVisibleOscillatorMin{0};
  int mVisibleOscillatorMax{SimplePreset::kNumOscillators - 1};
  OnOscillatorValueChangedFunc mOnOscillatorValueChanged{};
  IsOscillatorEditableFunc mIsOscillatorEditable{};
  GetOscillatorEditModeFunc mGetOscillatorEditMode{};
  ControlState mDrawLineSourceValues{};
  int mDrawLineStartOscillator{-1};
  double mDrawLineStartControlValue{0.0};
  bool mHasDrawLineStartPoint{false};
  double mPreviousStepwiseCursorValue{0.0};
  bool mHasPreviousStepwiseDragPoint{false};
};

} // namespace plugin_ui

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

class NoiseBandSliderControl final : public IVMultiSliderControl<NoiseBandProfile::kNumBands>
{
public:
  using Base = IVMultiSliderControl<NoiseBandProfile::kNumBands>;
  using OnBandValueChangedFunc = std::function<void(int bandIndex, double value)>;
  using GetBandEditModeFunc = std::function<EditorOscillatorEditMode()>;
  using ControlState = std::array<double, NoiseBandProfile::kNumBands>;
  using RestoreState = std::array<double, NoiseBandProfile::kNumBands>;

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

  explicit NoiseBandSliderControl(const IRECT& bounds,
                                  const char* label = "",
                                  const IVStyle& style = DEFAULT_STYLE,
                                  EDirection direction = EDirection::Vertical)
  : Base(bounds, label, style, 0, direction)
  {
    SetOnNewValueFunc([this](int bandIndex, double normalizedValue) {
      HandleBandValueChanged(bandIndex, normalizedValue);
    });
    ApplyBaseValue();
  }

  void SetConfig(const Config& config)
  {
    RestoreState bandValues{};
    for(int bandIndex = 0; bandIndex < NoiseBandProfile::kNumBands; ++bandIndex)
      bandValues[static_cast<std::size_t>(bandIndex)] = GetBandValue(bandIndex);

    mConfig = config;
    if(!std::isfinite(mConfig.range.min) || !std::isfinite(mConfig.range.max) || mConfig.range.max <= mConfig.range.min)
      mConfig.range = ValueRange{};

    ApplyBaseValue();
    for(int bandIndex = 0; bandIndex < NoiseBandProfile::kNumBands; ++bandIndex)
      SetBandValue(bandIndex, bandValues[static_cast<std::size_t>(bandIndex)]);

    SetDirty(false);
  }

  const Config& GetConfig() const
  {
    return mConfig;
  }

  void SetOnBandValueChanged(OnBandValueChangedFunc func)
  {
    mOnBandValueChanged = std::move(func);
  }

  void SetBandEditModeFunc(GetBandEditModeFunc func)
  {
    mGetBandEditMode = std::move(func);
  }

  void SetVisibleBandRange(int minBandOneBased, int maxBandOneBased)
  {
    int minBand = std::clamp(minBandOneBased - 1, 0, NoiseBandProfile::kNumBands - 1);
    int maxBand = std::clamp(maxBandOneBased - 1, 0, NoiseBandProfile::kNumBands - 1);
    if(maxBand < minBand)
      maxBand = minBand;

    if(mVisibleBandMin == minBand && mVisibleBandMax == maxBand)
      return;

    mVisibleBandMin = minBand;
    mVisibleBandMax = maxBand;
    OnResize();
  }

  void SetBandValue(int bandIndex, double value)
  {
    SetValue(ToControlValueFromRangeValue(value), bandIndex);
  }

  double GetBandValue(int bandIndex) const
  {
    return FromControlValueToRangeValue(GetValue(bandIndex));
  }

  void SetEditable(bool editable)
  {
    if(IsEditable() == editable)
      return;

    SetDisabled(!editable);
  }

  bool IsEditable() const
  {
    return !IsDisabled();
  }

  void CaptureRestoreState(int midiNote)
  {
    for(int bandIndex = 0; bandIndex < NoiseBandProfile::kNumBands; ++bandIndex)
      mRestoreState[static_cast<std::size_t>(bandIndex)] = GetBandValue(bandIndex);

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

    const int bandIndex = GetReadoutBandIndex();
    if(bandIndex < 0)
      return;

    DrawBandReadout(g, bandIndex);
  }

  void DrawPeak(IGraphics&, const IRECT&, int, bool) override
  {
  }

  void SnapToMouse(float x,
                   float y,
                   EDirection direction,
                   const IRECT& bounds,
                   int valIdx = -1,
                   double minClip = 0.,
                   double maxClip = 1.) override
  {
    const auto editPoint = GetMouseEditPoint(x, y, direction, bounds);
    const int sliderHit = editPoint.sliderHit;
    const double controlValue = editPoint.controlValue;
    bool changedValue = false;

    switch(GetBandEditMode())
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

    if(GetBandEditMode() == EditorOscillatorEditMode::DrawLine)
      BeginDrawLineDrag(GetMouseEditPoint(x, y, mDirection, mWidgetBounds));

    Base::OnMouseDown(x, y, mod);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    if(GetBandEditMode() == EditorOscillatorEditMode::DrawLine && mHasDrawLineStartPoint)
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

  float GetBackingPixelScale() const
  {
    if(const IGraphics* ui = GetUI())
      return std::max(1.f, ui->GetTotalScale());

    return 1.f;
  }

  static float GetSnappedSubdivisionEdge(float origin, float length, int index, int count, float scale)
  {
    const double scaledOrigin = static_cast<double>(origin) * static_cast<double>(scale);
    const double scaledLength = static_cast<double>(length) * static_cast<double>(scale);
    const double scaledEdge = scaledOrigin + (scaledLength * static_cast<double>(index)) / static_cast<double>(count);
    return static_cast<float>(std::round(scaledEdge) / static_cast<double>(scale));
  }

  EditorOscillatorEditMode GetBandEditMode() const
  {
    return mGetBandEditMode ? mGetBandEditMode() : EditorOscillatorEditMode::Set;
  }

  int FindBandIndexForPoint(float x, float y, EDirection direction) const
  {
    const int nVals = NVals();
    for(int bandIndex = 0; bandIndex < nVals; ++bandIndex)
    {
      const IRECT& trackBounds = mTrackBounds.Get()[bandIndex];
      if(trackBounds.Empty())
        continue;

      const bool isHit = direction == EDirection::Vertical
        ? trackBounds.ContainsEdge(x, trackBounds.MH())
        : trackBounds.ContainsEdge(trackBounds.MW(), y);
      if(isHit)
        return bandIndex;
    }

    return -1;
  }

  MouseEditPoint GetMouseEditPoint(float x,
                                   float y,
                                   EDirection direction,
                                   const IRECT& bounds) const
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

      sliderHit = FindBandIndexForPoint(constrainedX, constrainedY, direction);
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

      sliderHit = FindBandIndexForPoint(constrainedX, constrainedY, direction);
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
    mDrawLineStartBand = -1;
    mDrawLineStartControlValue = 0.0;
  }

  void BeginDrawLineDrag(const MouseEditPoint& startPoint)
  {
    if(startPoint.sliderHit < 0)
      return;

    for(int bandIndex = 0; bandIndex < NoiseBandProfile::kNumBands; ++bandIndex)
      mDrawLineSourceValues[static_cast<std::size_t>(bandIndex)] = GetValue(bandIndex);

    mDrawLineStartBand = startPoint.sliderHit;
    mDrawLineStartControlValue = startPoint.controlValue;
    mHasDrawLineStartPoint = true;
  }

  bool HandleSetDrag(int sliderHit, double controlValue)
  {
    bool changedValue = false;

    if(sliderHit > -1)
    {
      mMouseOverTrack = sliderHit;
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

          for(int bandIndex = lowBounds; bandIndex < highBounds; ++bandIndex)
          {
            const double frac = static_cast<double>(bandIndex - lowBounds)
                              / static_cast<double>(highBounds - lowBounds);
            SetValue(iplug::Lerp(GetValue(lowBounds), GetValue(highBounds), frac), bandIndex);
            OnNewValue(bandIndex, GetValue(bandIndex));
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

      for(int bandIndex = lowBounds; bandIndex <= highBounds; ++bandIndex)
      {
        if(bandIndex == previousSliderHit)
          continue;

        const double frac = span > 0
          ? static_cast<double>(std::abs(bandIndex - previousSliderHit)) / static_cast<double>(span)
          : 1.0;
        const double interpolatedCursorValue = iplug::Lerp(previousCursorValue, cursorControlValue, frac);
        changedValue = applyStep(bandIndex, interpolatedCursorValue) || changedValue;
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
      [this](int bandIndex, double stepCursorControlValue) {
        return ApplyNudgeStep(bandIndex, stepCursorControlValue);
      });
  }

  bool HandleSmoothDrag(int sliderHit, double cursorControlValue)
  {
    return HandleStepwiseDrag(
      sliderHit,
      cursorControlValue,
      [this](int bandIndex, double) {
        return ApplySmoothStep(bandIndex);
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

  bool ApplyNudgeStep(int bandIndex, double cursorControlValue)
  {
    const double currentControlValue = GetValue(bandIndex);
    double nudgedControlValue = currentControlValue;

    if(cursorControlValue > currentControlValue + kValueComparisonTolerance)
      nudgedControlValue = Clamp01(currentControlValue + kNudgeControlStep);
    else if(cursorControlValue < currentControlValue - kValueComparisonTolerance)
      nudgedControlValue = Clamp01(currentControlValue - kNudgeControlStep);
    else
      return false;

    if(std::fabs(nudgedControlValue - currentControlValue) <= kValueComparisonTolerance)
      return false;

    SetValue(nudgedControlValue, bandIndex);
    OnNewValue(bandIndex, nudgedControlValue);
    return true;
  }

  double GetSmoothedControlValue(int bandIndex) const
  {
    double weightedValue = 0.0;
    double totalWeight = 0.0;

    for(int offset = -3; offset <= 3; ++offset)
    {
      const int neighbourIndex = bandIndex + offset;
      if(neighbourIndex < 0 || neighbourIndex >= NoiseBandProfile::kNumBands)
        continue;

      const double weight = kSmoothControlKernel[static_cast<std::size_t>(offset + 3)];
      weightedValue += GetValue(neighbourIndex) * weight;
      totalWeight += weight;
    }

    if(totalWeight <= 0.0)
      return GetValue(bandIndex);

    return Clamp01(weightedValue / totalWeight);
  }

  bool ApplySmoothStep(int bandIndex)
  {
    const double currentControlValue = GetValue(bandIndex);
    const double smoothedControlValue = GetSmoothedControlValue(bandIndex);
    if(std::fabs(smoothedControlValue - currentControlValue) <= kValueComparisonTolerance)
      return false;

    SetValue(smoothedControlValue, bandIndex);
    OnNewValue(bandIndex, smoothedControlValue);
    return true;
  }

  bool ApplyDrawLinePreview(int endBandIndex, double endControlValue)
  {
    if(mDrawLineStartBand < 0)
      return false;

    ControlState desiredValues = mDrawLineSourceValues;
    const int startBandIndex = mDrawLineStartBand;
    const double startControlValue = mDrawLineStartControlValue;

    if(startBandIndex == endBandIndex)
    {
      desiredValues[static_cast<std::size_t>(startBandIndex)] = Clamp01((startControlValue + endControlValue) * 0.5);
    }
    else
    {
      const int span = std::abs(endBandIndex - startBandIndex);
      const int lowBounds = std::min(startBandIndex, endBandIndex);
      const int highBounds = std::max(startBandIndex, endBandIndex);

      for(int bandIndex = lowBounds; bandIndex <= highBounds; ++bandIndex)
      {
        const double frac = static_cast<double>(std::abs(bandIndex - startBandIndex)) / static_cast<double>(span);
        desiredValues[static_cast<std::size_t>(bandIndex)] =
          Clamp01(iplug::Lerp(startControlValue, endControlValue, frac));
      }
    }

    return ApplyControlState(desiredValues);
  }

  bool ApplyControlState(const ControlState& desiredValues)
  {
    bool changedValue = false;

    for(int bandIndex = 0; bandIndex < NoiseBandProfile::kNumBands; ++bandIndex)
    {
      const double currentControlValue = GetValue(bandIndex);
      const double desiredControlValue = Clamp01(desiredValues[static_cast<std::size_t>(bandIndex)]);
      if(std::fabs(desiredControlValue - currentControlValue) <= kValueComparisonTolerance)
        continue;

      SetValue(desiredControlValue, bandIndex);
      OnNewValue(bandIndex, desiredControlValue);
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

  void HandleBandValueChanged(int bandIndex, double normalizedValue)
  {
    if(mOnBandValueChanged)
      mOnBandValueChanged(bandIndex, FromControlValueToRangeValue(normalizedValue));
  }

  void ApplyBaseValue()
  {
    if(mConfig.range.min < 0.0 && mConfig.range.max > 0.0)
      SetBaseValue(ToControlValueFromRangeValue(0.0));
    else
      SetBaseValue(0.0);
  }

  int GetReadoutBandIndex() const
  {
    if(IsVisibleBandIndex(mMouseOverTrack))
      return mMouseOverTrack;

    if(IsVisibleBandIndex(mHighlightedTrack))
      return mHighlightedTrack;

    return -1;
  }

  bool IsVisibleBandIndex(int bandIndex) const
  {
    return bandIndex >= 0
      && bandIndex < NVals()
      && !mTrackBounds.Get()[bandIndex].Empty();
  }

  void DrawBandReadout(IGraphics& g, int bandIndex) const
  {
    const IRECT& trackBounds = mTrackBounds.Get()[bandIndex];
    if(trackBounds.Empty())
      return;

    const float readoutBandHeight = std::max(0.f, GetReadoutBandHeight());
    if(readoutBandHeight <= 0.f)
      return;

    const float centerX = trackBounds.MW();
    const IRECT topBand = MakeReadoutRect(IRECT(mRECT.L, mRECT.T, mRECT.R, mWidgetBounds.T), centerX);
    const IRECT bottomBand = MakeReadoutRect(IRECT(mRECT.L, mWidgetBounds.B, mRECT.R, mRECT.B), centerX);

    const WDL_String centerFrequency = FormatCenterFrequency(bandIndex);
    const WDL_String value = FormatBandValue(bandIndex);

    g.DrawText(MakeReadoutText("Roboto-Black", 12.f, EAlign::Center, EVAlign::Bottom), value.Get(), topBand, &mBlend);
    g.DrawText(MakeReadoutText("Roboto-Black", 11.f, EAlign::Center, EVAlign::Top), centerFrequency.Get(), bottomBand, &mBlend);
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

  WDL_String FormatFrequency(double frequencyHz) const
  {
    WDL_String text;
    if(frequencyHz >= 1000.0)
    {
      const double kilohertz = frequencyHz / 1000.0;
      if(kilohertz >= 10.0)
        text.SetFormatted(16, "%.0fk", kilohertz);
      else
        text.SetFormatted(16, "%.1fk", kilohertz);
      return text;
    }

    if(frequencyHz >= 100.0)
      text.SetFormatted(16, "%.0f", frequencyHz);
    else
      text.SetFormatted(16, "%.1f", frequencyHz);

    return text;
  }

  WDL_String FormatCenterFrequency(int bandIndex) const
  {
    return FormatFrequency(NoiseBandProfile::GetBandCenterFrequencyHz(bandIndex));
  }

  WDL_String FormatBandValue(int bandIndex) const
  {
    const double value = GetBandValue(bandIndex);
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

  void MakeTrackRects(const IRECT& bounds) override
  {
    const int nVals = NVals();
    const int dir = static_cast<int>(mDirection);
    const float scale = GetBackingPixelScale();
    const float gapSize = 2.f / scale;

    for(int bandIndex = 0; bandIndex < nVals; ++bandIndex)
      mTrackBounds.Get()[bandIndex] = IRECT();

    const int visibleMin = std::clamp(mVisibleBandMin, 0, nVals - 1);
    const int visibleMax = std::clamp(mVisibleBandMax, visibleMin, nVals - 1);
    const int visibleCount = visibleMax - visibleMin + 1;

    for(int bandIndex = visibleMin; bandIndex <= visibleMax; ++bandIndex)
    {
      const int visibleIndex = bandIndex - visibleMin;
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

      if(bandIndex < visibleMax)
      {
        if(mDirection == EDirection::Horizontal)
          trackBounds.B = std::max(trackBounds.T, trackBounds.B - gapSize);
        else
          trackBounds.R = std::max(trackBounds.L, trackBounds.R - gapSize);
      }

      mTrackBounds.Get()[bandIndex] = trackBounds;
    }
  }

  Config mConfig{};
  RestoreState mRestoreState{};
  bool mHasRestoreState{false};
  static constexpr int kNoRestoreMidiNote = std::numeric_limits<int>::min();
  int mRestoreMidiNote{kNoRestoreMidiNote};
  int mVisibleBandMin{0};
  int mVisibleBandMax{NoiseBandProfile::kNumBands - 1};
  OnBandValueChangedFunc mOnBandValueChanged{};
  GetBandEditModeFunc mGetBandEditMode{};
  ControlState mDrawLineSourceValues{};
  int mDrawLineStartBand{-1};
  double mDrawLineStartControlValue{0.0};
  bool mHasDrawLineStartPoint{false};
  double mPreviousStepwiseCursorValue{0.0};
  bool mHasPreviousStepwiseDragPoint{false};
};

} // namespace plugin_ui

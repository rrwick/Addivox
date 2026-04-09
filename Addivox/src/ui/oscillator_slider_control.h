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
    const float sidePadding = 15.f;
    mWidgetBounds = plotBounds.GetReducedFromLeft(sidePadding).GetReducedFromRight(sidePadding);
    MakeTrackRects(mWidgetBounds);
    MakeStepRects(mWidgetBounds, mNSteps);
    SetDirty(false);
  }

  void DrawWidget(IGraphics& g) override
  {
    Base::DrawWidget(g);

    const int oscillatorIndex = GetReadoutOscillatorIndex();
    if(oscillatorIndex < 0)
      return;

    DrawOscillatorReadout(g, oscillatorIndex);
  }

  void SnapToMouse(float x,
                   float y,
                   EDirection direction,
                   const IRECT& bounds,
                   int valIdx = -1,
                   double minClip = 0.,
                   double maxClip = 1.) override
  {
    bounds.Constrain(x, y);
    const int nVals = NVals();

    double value = 0.;
    int sliderHit = -1;

    const int step = GetStepIdxForPos(x, y);

    if(direction == EDirection::Vertical)
    {
      if(step > -1)
      {
        y = mStepBounds.Get()[step].T;

        if(mStepBounds.GetSize() == 1)
          value = 1.f;
        else
          value = step * (1.f / float(mStepBounds.GetSize() - 1));
      }
      else
      {
        value = 1.f - (y - bounds.T) / bounds.H();
      }

      for(int oscillatorIndex = 0; oscillatorIndex < nVals; ++oscillatorIndex)
      {
        if(mTrackBounds.Get()[oscillatorIndex].ContainsEdge(x, mTrackBounds.Get()[oscillatorIndex].MH()))
        {
          sliderHit = oscillatorIndex;
          break;
        }
      }
    }
    else
    {
      if(step > -1)
      {
        x = mStepBounds.Get()[step].L;

        if(mStepBounds.GetSize() == 1)
          value = 1.f;
        else
          value = 1.f - (step * (1.f / float(mStepBounds.GetSize() - 1)));
      }
      else
      {
        value = (x - bounds.L) / bounds.W();
      }

      for(int oscillatorIndex = 0; oscillatorIndex < nVals; ++oscillatorIndex)
      {
        if(mTrackBounds.Get()[oscillatorIndex].ContainsEdge(mTrackBounds.Get()[oscillatorIndex].MW(), y))
        {
          sliderHit = oscillatorIndex;
          break;
        }
      }
    }

    if(!GetStepped())
      value = std::round(value / mGrain) * mGrain;

    const double controlValue = std::clamp(value, 0.0, 1.0);
    bool changedValue = false;

    switch(GetOscillatorEditMode())
    {
      case EditorOscillatorEditMode::Nudge:
        changedValue = HandleNudgeDrag(sliderHit, controlValue);
        break;
      case EditorOscillatorEditMode::Smooth:
        changedValue = HandleSmoothDrag(sliderHit, controlValue);
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
    mHasPreviousStepwiseDragPoint = false;
    Base::OnMouseDown(x, y, mod);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    mHasPreviousStepwiseDragPoint = false;
    Base::OnMouseUp(x, y, mod);
  }

private:
  static constexpr float kReadoutMaxBandHeight = 16.f;
  static constexpr float kReadoutMinBandHeight = 8.f;
  static constexpr float kReadoutTextWidth = 72.f;
  static constexpr double kNudgeControlStep = 0.005;
  static constexpr double kValueComparisonTolerance = 1.0e-9;
  static constexpr std::array<double, 7> kSmoothControlKernel{{0.01, 0.05, 0.10, 0.68, 0.10, 0.05, 0.01}};

  static double Clamp01(double value)
  {
    return std::clamp(value, 0.0, 1.0);
  }

  static double ClampSignedUnit(double value)
  {
    return std::clamp(value, -1.0, 1.0);
  }

  EditorOscillatorEditMode GetOscillatorEditMode() const
  {
    return mGetOscillatorEditMode ? mGetOscillatorEditMode() : EditorOscillatorEditMode::Set;
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

  bool UsesMirroredBipolarTransform() const
  {
    if(mConfig.range.min >= 0.0 || mConfig.range.max <= 0.0)
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

    for(int ch = 0; ch < nVals; ++ch)
      mTrackBounds.Get()[ch] = IRECT();

    const int visibleMin = std::clamp(mVisibleOscillatorMin, 0, nVals - 1);
    const int visibleMax = std::clamp(mVisibleOscillatorMax, visibleMin, nVals - 1);
    const int visibleCount = visibleMax - visibleMin + 1;

    for(int ch = visibleMin; ch <= visibleMax; ++ch)
    {
      const int visibleIndex = ch - visibleMin;
      mTrackBounds.Get()[ch] = bounds.SubRect(EDirection(!dir), visibleCount, visibleIndex)
                                     .GetPadded(0, -mTrackPadding * static_cast<float>(dir),
                                                -mTrackPadding * static_cast<float>(!dir), -mTrackPadding);
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
  double mPreviousStepwiseCursorValue{0.0};
  bool mHasPreviousStepwiseDragPoint{false};
};

} // namespace plugin_ui

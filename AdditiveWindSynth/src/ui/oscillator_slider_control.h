#pragma once

#include "IControls.h"
#include "../settings/settings_oscillator.h"

#include <algorithm>
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
  using RestoreState = std::array<double, SimplePreset::kNumOscillators>;

  enum class ValueTransform
  {
    Linear
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
    mConfig = config;
    if(!std::isfinite(mConfig.range.min) || !std::isfinite(mConfig.range.max) || mConfig.range.max <= mConfig.range.min)
      mConfig.range = ValueRange{};

    ApplyBaseValue();
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
    SetValue(ToControlValue(Normalize(value)), oscillatorIndex);
  }

  double GetOscillatorValue(int oscillatorIndex) const
  {
    return Denormalize(FromControlValue(GetValue(oscillatorIndex)));
  }

  void SetEditable(bool editable)
  {
    SetDisabled(!editable);
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

private:
  static double Clamp01(double value)
  {
    return std::clamp(value, 0.0, 1.0);
  }

  double ToControlValue(double normalizedValue) const
  {
    const double clamped = Clamp01(normalizedValue);
    switch(mConfig.transform)
    {
      case ValueTransform::Linear:
      default:
        return clamped;
    }
  }

  double FromControlValue(double controlValue) const
  {
    const double clamped = Clamp01(controlValue);
    switch(mConfig.transform)
    {
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

  void HandleSliderValueChanged(int oscillatorIndex, double normalizedValue)
  {
    if(mOnOscillatorValueChanged)
      mOnOscillatorValueChanged(oscillatorIndex, Denormalize(FromControlValue(normalizedValue)));
  }

  void ApplyBaseValue()
  {
    const bool bipolarAroundZero = mConfig.range.min < 0.0 && mConfig.range.max > 0.0;
    if(bipolarAroundZero)
      SetBaseValue(ToControlValue(Normalize(0.0)));
    else
      SetBaseValue(0.0);
  }

  Config mConfig{};
  RestoreState mRestoreState{};
  bool mHasRestoreState{false};
  static constexpr int kNoRestoreMidiNote = std::numeric_limits<int>::min();
  int mRestoreMidiNote{kNoRestoreMidiNote};
  OnOscillatorValueChangedFunc mOnOscillatorValueChanged{};
};

} // namespace plugin_ui

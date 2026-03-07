#pragma once

#include "IControls.h"
#include "../control_utils.h"
#include "../action_selection_control.h"
#include "../editor_state.h"
#include "../oscillator_slider_control.h"
#include "../theme.h"
#include "../../editor_messages.h"
#include "../../settings/settings_oscillator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace editor
{
using OscillatorParameter = OscillatorSettings::Parameter;
using SliderRange = OscillatorSliderControl::ValueRange;
using EditorStyles = theme::EditorStyles;

struct OscillatorTabDescriptor
{
  const char* title;
  OscillatorParameter parameter;
  SliderRange range;
  const char* description;
};

// IVTabbedPagesControl stores pages in a std::map<const char*, ...>, so ordering follows
// pointer values rather than insertion order. Keeping all titles in one contiguous buffer
// makes the pointer order deterministic and match the desired tab order.
inline constexpr char kOscillatorTabTitleStorage[] =
  "Level\0"
  "Breath\0"
  "Attack\0"
  "Release\0"
  "Pitch\0"
  "Pan\0"
  "LvlVarAmt\0"
  "LvlVarRate\0"
  "PchVarAmt\0"
  "PchVarRate\0"
  "PanVarAmt\0"
  "PanVarRate\0";

inline constexpr std::array<const char*, OscillatorSettings::kNumParameters> kOscillatorTabTitles{{
  kOscillatorTabTitleStorage + 0,
  kOscillatorTabTitleStorage + 6,
  kOscillatorTabTitleStorage + 13,
  kOscillatorTabTitleStorage + 20,
  kOscillatorTabTitleStorage + 28,
  kOscillatorTabTitleStorage + 34,
  kOscillatorTabTitleStorage + 38,
  kOscillatorTabTitleStorage + 48,
  kOscillatorTabTitleStorage + 59,
  kOscillatorTabTitleStorage + 69,
  kOscillatorTabTitleStorage + 80,
  kOscillatorTabTitleStorage + 90,
}};

inline const char* GetLevelTransformLabel(EditorLevelTransform transform)
{
  switch(transform)
  {
    case EditorLevelTransform::SquareRoot:
      return "square root";
    case EditorLevelTransform::PseudoLog:
      return "pseudo-log";
    case EditorLevelTransform::Linear:
    default:
      return "linear";
  }
}

inline OscillatorSliderControl::ValueTransform GetSliderValueTransform(EditorLevelTransform transform)
{
  switch(transform)
  {
    case EditorLevelTransform::SquareRoot:
      return OscillatorSliderControl::ValueTransform::SquareRoot;
    case EditorLevelTransform::PseudoLog:
      return OscillatorSliderControl::ValueTransform::PseudoLog;
    case EditorLevelTransform::Linear:
    default:
      return OscillatorSliderControl::ValueTransform::Linear;
  }
}

inline void SetDisabledState(IControl* control, bool disabled)
{
  if(!control)
    return;

  control->SetDisabled(disabled);
  control->SetDirty(false);
}

inline int RoundOscillatorRangeValue(double value)
{
  return std::clamp(static_cast<int>(std::lround(value)), 1, SimplePreset::kNumOscillators);
}

const std::vector<OscillatorTabDescriptor>& GetOscillatorTabDescriptors();

struct EditorModelRefs
{
  std::shared_ptr<CompoundPreset> compoundPreset;
  std::shared_ptr<int> selectedMidiNote;
  std::shared_ptr<int> selectedTabIndex;
  std::shared_ptr<bool> editMode;
};

struct OscillatorViewRefs
{
  std::shared_ptr<int> xRangeMin;
  std::shared_ptr<int> xRangeMax;
};

struct LevelTabRefs
{
  std::shared_ptr<EditorLevelTransform> levelTransform;
  std::shared_ptr<ActionSelectionControl*> setShapeControl;
  std::shared_ptr<ActionSelectionControl*> actionsControl;
};

struct BreathTabRefs
{
  std::shared_ptr<EditorLevelTransform> breathTransform;
  std::shared_ptr<ActionSelectionControl*> setShapeControl;
};

struct OscillatorTabControlRefs
{
  std::shared_ptr<std::array<OscillatorSliderControl*, OscillatorSettings::kNumParameters>> sliderControls;
  std::shared_ptr<std::array<IVNumberBoxControl*, OscillatorSettings::kNumParameters>> xRangeMinControls;
  std::shared_ptr<std::array<IVNumberBoxControl*, OscillatorSettings::kNumParameters>> xRangeMaxControls;
  std::shared_ptr<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>> restoreButtons;
};

struct EditorButtonRefs
{
  std::shared_ptr<IVButtonControl*> addButton;
  std::shared_ptr<IVButtonControl*> deleteButton;
};

struct EditorContext
{
  int editorTabsTag = kNoTag;
  EditorModelRefs model;
  OscillatorViewRefs oscillatorView;
  LevelTabRefs levelTab;
  BreathTabRefs breathTab;
  OscillatorTabControlRefs oscillatorTabControls;
  EditorButtonRefs buttons;

  CompoundPreset& Preset() const
  {
    return *model.compoundPreset;
  }

  int SelectedMidiNote() const
  {
    return *model.selectedMidiNote;
  }

  void SetSelectedMidiNote(int midiNote) const
  {
    *model.selectedMidiNote = midiNote;
  }

  bool IsEditMode() const
  {
    return *model.editMode;
  }

  void SetEditMode(bool editMode) const
  {
    *model.editMode = editMode;
  }

  int XRangeMin() const
  {
    return *oscillatorView.xRangeMin;
  }

  int XRangeMax() const
  {
    return *oscillatorView.xRangeMax;
  }

  void SetXRange(int minOscillator, int maxOscillator) const
  {
    *oscillatorView.xRangeMin = minOscillator;
    *oscillatorView.xRangeMax = maxOscillator;
  }

  bool HasValidSelectedMidiNote() const
  {
    const int midiNote = SelectedMidiNote();
    return midiNote >= CompoundPreset::kMinMidiNote && midiNote <= CompoundPreset::kMaxMidiNote;
  }

  void ApplyVisibleOscillatorRangeToSliders() const
  {
    for(auto* control : *oscillatorTabControls.sliderControls)
    {
      if(control)
        control->SetVisibleOscillatorRange(XRangeMin(), XRangeMax());
    }
  }

  void SyncXRangeNumberBoxes() const
  {
    const auto setNumberBoxValueSilently = [](IVNumberBoxControl* control, int value) {
      if(!control || RoundOscillatorRangeValue(control->GetRealValue()) == value)
        return;

      const auto originalAction = control->GetActionFunction();
      control->SetActionFunction(nullptr);

      WDL_String textValue;
      textValue.SetFormatted(16, "%d", value);
      control->OnTextEntryCompletion(textValue.Get(), 0);
      control->SetActionFunction(originalAction);
      control->SetDirty(false);
    };

    for(std::size_t i = 0; i < oscillatorTabControls.xRangeMinControls->size(); ++i)
    {
      setNumberBoxValueSilently((*oscillatorTabControls.xRangeMinControls)[i], XRangeMin());
      setNumberBoxValueSilently((*oscillatorTabControls.xRangeMaxControls)[i], XRangeMax());
    }
  }

  void SendOscillatorParameterToDSP(IControl* sourceControl,
                                    int midiNote,
                                    int oscillatorIndex,
                                    OscillatorParameter parameter,
                                    double value) const
  {
    if(!sourceControl)
      return;

    if(auto* delegate = sourceControl->GetDelegate())
    {
      editor_messages::SetKeyNoteOscillatorParameterPayload payload;
      payload.midiNote = midiNote;
      payload.oscillatorIndex = oscillatorIndex;
      payload.parameter = static_cast<int>(parameter);
      payload.value = value;
      delegate->SendArbitraryMsgFromUI(
        editor_messages::kMsgTagSetKeyNoteOscillatorParameter,
        editorTabsTag,
        sizeof(payload),
        &payload);
    }
  }

  void SendOscillatorParameterValuesToDSP(IControl* sourceControl,
                                          int midiNote,
                                          OscillatorParameter parameter,
                                          const std::array<double, SimplePreset::kNumOscillators>& values) const
  {
    if(!sourceControl)
      return;

    if(auto* delegate = sourceControl->GetDelegate())
    {
      editor_messages::SetKeyNoteOscillatorParameterValuesPayload payload;
      payload.midiNote = midiNote;
      payload.parameter = static_cast<int>(parameter);
      payload.values = values;
      delegate->SendArbitraryMsgFromUI(
        editor_messages::kMsgTagSetKeyNoteOscillatorParameterValues,
        editorTabsTag,
        sizeof(payload),
        &payload);
    }
  }

  void SendKeyNotePresetEditToDSP(IControl* sourceControl, int msgTag, int midiNote) const
  {
    if(!sourceControl)
      return;

    if(auto* delegate = sourceControl->GetDelegate())
    {
      editor_messages::KeyNotePresetPayload payload;
      payload.midiNote = midiNote;
      delegate->SendArbitraryMsgFromUI(msgTag, editorTabsTag, sizeof(payload), &payload);
    }
  }

  void RefreshEditorActionButtons() const
  {
    const int midiNote = SelectedMidiNote();
    const bool midiNoteValid = midiNote >= CompoundPreset::kMinMidiNote && midiNote <= CompoundPreset::kMaxMidiNote;
    const bool keyNoteSelected = midiNoteValid && Preset().HasKeyNotePreset(midiNote);
    const bool canRemoveKeyNote = Preset().GetNumKeyNotePresets() > 1;

    SetDisabledState(*buttons.addButton, !(IsEditMode() && midiNoteValid && !keyNoteSelected));
    SetDisabledState(*buttons.deleteButton, !(IsEditMode() && keyNoteSelected && canRemoveKeyNote));
  }

  void RefreshOscillatorTabs() const
  {
    ApplyVisibleOscillatorRangeToSliders();
    SyncXRangeNumberBoxes();

    if(!HasValidSelectedMidiNote())
    {
      for(std::size_t i = 0; i < oscillatorTabControls.sliderControls->size(); ++i)
      {
        if(auto* control = (*oscillatorTabControls.sliderControls)[i])
        {
          control->SetEditable(false);
          control->SetDirty(false);
        }

        SetDisabledState((*oscillatorTabControls.restoreButtons)[i], true);
      }

      SetDisabledState(*levelTab.setShapeControl, true);
      SetDisabledState(*levelTab.actionsControl, true);
      SetDisabledState(*breathTab.setShapeControl, true);
      return;
    }

    const int midiNote = SelectedMidiNote();
    const SimplePreset* keyNotePreset = Preset().GetKeyNotePreset(midiNote);
    const SimplePreset& selectedPreset = keyNotePreset ? *keyNotePreset : Preset().GetPresetForMidiNote(midiNote);
    const bool editable = keyNotePreset != nullptr;

    SetDisabledState(*levelTab.setShapeControl, !editable);
    SetDisabledState(*levelTab.actionsControl, !editable);
    SetDisabledState(*breathTab.setShapeControl, !editable);

    for(const auto& descriptor : GetOscillatorTabDescriptors())
    {
      auto* control = (*oscillatorTabControls.sliderControls)[static_cast<std::size_t>(descriptor.parameter)];
      if(!control)
        continue;

      for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
      {
        const double value = selectedPreset.GetOscillatorSettings(oscillatorIndex).GetParameter(descriptor.parameter);
        control->SetOscillatorValue(oscillatorIndex, value);
      }

      if(!control->IsHidden() && !control->HasRestoreStateForMidiNote(midiNote))
        control->CaptureRestoreState(midiNote);

      control->SetEditable(editable);
      control->SetDirty(false);

      auto* restoreButton = (*oscillatorTabControls.restoreButtons)[static_cast<std::size_t>(descriptor.parameter)];
      SetDisabledState(restoreButton, !(editable && control->HasRestoreStateForMidiNote(midiNote)));
    }
  }

  template <typename Action>
  void ApplyOscillatorParameterActionToSelectedKeyNote(OscillatorSliderControl* control,
                                                       OscillatorParameter parameter,
                                                       Action&& action) const
  {
    const int midiNote = SelectedMidiNote();
    const SimplePreset* keyNotePreset = Preset().GetKeyNotePreset(midiNote);
    if(!keyNotePreset)
      return;

    SimplePreset updatedPreset = *keyNotePreset;
    if(!std::forward<Action>(action)(updatedPreset))
      return;

    std::array<double, SimplePreset::kNumOscillators> values{};
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
    {
      values[static_cast<std::size_t>(oscillatorIndex)] =
        updatedPreset.GetOscillatorSettings(oscillatorIndex).GetParameter(parameter);
    }

    Preset().SetKeyNotePreset(midiNote, updatedPreset);
    SendOscillatorParameterValuesToDSP(control, midiNote, parameter, values);
    RefreshOscillatorTabs();
  }
};

struct XRangeControls
{
  IVNumberBoxControl* minControl{};
  IVNumberBoxControl* maxControl{};
};

inline void ResizeDefaultOscillatorTabPage(IContainerBase* pTab, const IRECT& r)
{
  if(pTab->NChildren() < 6)
    return;

  constexpr float kLeftInset = 104.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = 24.f;
  constexpr float kDescriptionHeight = 64.f;
  constexpr float kButtonHeight = 24.f;
  constexpr float kBottomPad = 8.f;
  constexpr float kTightGap = 4.f;
  constexpr float kHalfGap = 6.f;
  constexpr float kXRangeBlockHeight = kLabelHeight + kTightGap + kControlHeight;

  auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
  auto leftColumnBounds = innerBounds.GetFromLeft(kLeftInset);
  const float restoreTop = leftColumnBounds.B - (kButtonHeight + kBottomPad);
  auto restoreButtonBounds = IRECT(leftColumnBounds.L + 8.f, restoreTop, leftColumnBounds.R - 8.f, leftColumnBounds.B - kBottomPad);
  auto descriptionBounds = IRECT(
    leftColumnBounds.L + 4.f,
    leftColumnBounds.T + 2.f,
    leftColumnBounds.R - 4.f,
    leftColumnBounds.T + 2.f + kDescriptionHeight);
  const float controlsTop = std::min(descriptionBounds.B, restoreTop - kXRangeBlockHeight);
  const float rowL = leftColumnBounds.L + 8.f;
  const float rowR = leftColumnBounds.R - 8.f;
  const float rowMid = (rowL + rowR) * 0.5f;
  auto xRangeLabelBounds = IRECT(rowL, controlsTop, rowR, controlsTop + kLabelHeight);
  auto xRangeMinBounds = IRECT(rowL, xRangeLabelBounds.B + kTightGap, rowMid - kHalfGap * 0.5f, xRangeLabelBounds.B + kTightGap + kControlHeight);
  auto xRangeMaxBounds = IRECT(rowMid + kHalfGap * 0.5f, xRangeMinBounds.T, rowR, xRangeMinBounds.B);
  auto sliderBounds = innerBounds;
  sliderBounds.L += kLeftInset;

  pTab->GetChild(0)->SetTargetAndDrawRECTs(descriptionBounds);
  pTab->GetChild(1)->SetTargetAndDrawRECTs(xRangeLabelBounds);
  pTab->GetChild(2)->SetTargetAndDrawRECTs(xRangeMinBounds);
  pTab->GetChild(3)->SetTargetAndDrawRECTs(xRangeMaxBounds);
  pTab->GetChild(4)->SetTargetAndDrawRECTs(restoreButtonBounds);
  pTab->GetChild(5)->SetTargetAndDrawRECTs(sliderBounds);
}

inline void RestoreOscillatorTabValues(const std::shared_ptr<EditorContext>& context,
                                       IControl* caller,
                                       const OscillatorTabDescriptor& descriptor)
{
  if(!caller || !context->HasValidSelectedMidiNote())
    return;

  const int midiNote = context->SelectedMidiNote();
  if(!context->Preset().HasKeyNotePreset(midiNote))
    return;

  auto* control = (*context->oscillatorTabControls.sliderControls)[static_cast<std::size_t>(descriptor.parameter)];
  if(!control || !control->HasRestoreStateForMidiNote(midiNote))
    return;

  const auto& restoreState = control->GetRestoreState();
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    const double value = std::clamp(
      restoreState[static_cast<std::size_t>(oscillatorIndex)],
      descriptor.range.min,
      descriptor.range.max);
    const bool updated = context->Preset().SetKeyNoteOscillatorParameter(
      midiNote,
      oscillatorIndex,
      descriptor.parameter,
      value);
    if(!updated)
      return;

    context->SendOscillatorParameterToDSP(caller, midiNote, oscillatorIndex, descriptor.parameter, value);
  }

  context->RefreshOscillatorTabs();
}

inline OscillatorSliderControl* CreateOscillatorSliderControl(const std::shared_ptr<EditorContext>& context,
                                                              const OscillatorTabDescriptor& descriptor,
                                                              const EditorStyles& styles)
{
  auto* control = new OscillatorSliderControl(IRECT(), "", styles.sliderStyle, EDirection::Vertical);
  OscillatorSliderControl::Config config;
  config.range = descriptor.range;
  if(descriptor.parameter == OscillatorParameter::intensity)
    config.transform = GetSliderValueTransform(*context->levelTab.levelTransform);
  else if(descriptor.parameter == OscillatorParameter::breath_power)
    config.transform = GetSliderValueTransform(*context->breathTab.breathTransform);
  control->SetConfig(config);
  control->SetVisibleOscillatorRange(context->XRangeMin(), context->XRangeMax());
  control->SetOnOscillatorValueChanged(
    [context, control, descriptor](int oscillatorIndex, double value) {
      if(oscillatorIndex < 0 || oscillatorIndex >= SimplePreset::kNumOscillators)
        return;

      const int midiNote = context->SelectedMidiNote();
      const double clampedValue = std::clamp(value, descriptor.range.min, descriptor.range.max);
      const bool updated = context->Preset().SetKeyNoteOscillatorParameter(
        midiNote,
        oscillatorIndex,
        descriptor.parameter,
        clampedValue);
      if(!updated)
        return;

      context->SendOscillatorParameterToDSP(control, midiNote, oscillatorIndex, descriptor.parameter, clampedValue);
      context->RefreshOscillatorTabs();
    });
  return control;
}

inline XRangeControls CreateXRangeControls(const std::shared_ptr<EditorContext>& context,
                                           const OscillatorTabDescriptor& descriptor,
                                           const EditorStyles& styles)
{
  auto* minControl = new IVNumberBoxControl(
    IRECT(), kNoParameter, nullptr, "", styles.utilityNumberBoxStyle, false,
    static_cast<double>(context->XRangeMin()), 1.0, static_cast<double>(SimplePreset::kNumOscillators), "%0.0f", false);
  auto* maxControl = new IVNumberBoxControl(
    IRECT(), kNoParameter, nullptr, "", styles.utilityNumberBoxStyle, false,
    static_cast<double>(context->XRangeMax()), 1.0, static_cast<double>(SimplePreset::kNumOscillators), "%0.0f", false);
  minControl->SetDrawTriangle(false);
  maxControl->SetDrawTriangle(false);

  auto rangeGuard = std::make_shared<bool>(false);
  const auto clampEditedControl = [rangeGuard, minControl, maxControl](IVNumberBoxControl* editedControl) {
    if(*rangeGuard)
      return;

    const double minValue = minControl->GetRealValue();
    const double maxValue = maxControl->GetRealValue();
    if(minValue <= maxValue)
      return;

    *rangeGuard = true;
    WDL_String textValue;
    textValue.SetFormatted(16, "%0.0f", editedControl == minControl ? maxValue : minValue);
    editedControl->OnTextEntryCompletion(textValue.Get(), 0);
    *rangeGuard = false;
  };
  const auto updateVisibleRange = [context, minControl, maxControl]() {
    context->SetXRange(RoundOscillatorRangeValue(minControl->GetRealValue()),
                       RoundOscillatorRangeValue(maxControl->GetRealValue()));
    context->ApplyVisibleOscillatorRangeToSliders();
    context->SyncXRangeNumberBoxes();
  };

  minControl->SetActionFunction([clampEditedControl, updateVisibleRange, minControl](IControl*) {
    clampEditedControl(minControl);
    updateVisibleRange();
  });
  maxControl->SetActionFunction([clampEditedControl, updateVisibleRange, maxControl](IControl*) {
    clampEditedControl(maxControl);
    updateVisibleRange();
  });

  const auto parameterIndex = static_cast<std::size_t>(descriptor.parameter);
  (*context->oscillatorTabControls.xRangeMinControls)[parameterIndex] = minControl;
  (*context->oscillatorTabControls.xRangeMaxControls)[parameterIndex] = maxControl;
  context->SyncXRangeNumberBoxes();
  return {minControl, maxControl};
}
} // namespace editor
} // namespace plugin_ui

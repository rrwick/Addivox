#pragma once

#include "noise_sustain.h"

namespace editor
{
inline bool TryApplyNoiseAttackShape(NoiseBandProfile& profile, const char* shapeName)
{
  return TryApplyNoiseSustainShape(profile, shapeName);
}

inline bool ApplyNoiseAttackAction(NoiseBandProfile& profile, const char* actionName)
{
  return ApplyNoiseSustainAction(profile, actionName);
}

inline ActionSelectionControl* CreateNoiseAttackYTransformControl(const std::shared_ptr<EditorLevelTransform>& transform,
                                                                  NoiseBandSliderControl* sliderControl,
                                                                  const EditorStyles& styles)
{
  auto* control = new ActionSelectionControl(
    IRECT(),
    GetLevelTransformLabel(*transform),
    {"linear", "square root", "pseudo-log"},
    styles.utilityDropdownText,
    styles.darkTab,
    true);
  control->SetOnSelection([transform, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    if(std::strcmp(selectedText, "square root") == 0)
      *transform = EditorLevelTransform::SquareRoot;
    else if(std::strcmp(selectedText, "pseudo-log") == 0)
      *transform = EditorLevelTransform::PseudoLog;
    else
      *transform = EditorLevelTransform::Linear;

    auto config = sliderControl->GetConfig();
    config.transform = GetNoiseBandSliderValueTransform(*transform);
    sliderControl->SetConfig(config);
  });
  return control;
}

inline ActionSelectionControl* CreateNoiseAttackEditModeControl(const std::shared_ptr<EditorOscillatorEditMode>& editMode,
                                                                const EditorStyles& styles)
{
  auto* control = new ActionSelectionControl(
    IRECT(),
    GetOscillatorEditModeLabel(*editMode),
    {"set", "nudge", "smooth", "draw line"},
    styles.utilityDropdownText,
    styles.darkTab,
    true);
  control->SetTooltip(help_text::oscillator_tabs::kNoiseBandEditMode);
  control->SetOnSelection([editMode](const char* selectedText) {
    if(!selectedText)
      return;

    *editMode = GetOscillatorEditModeFromLabel(selectedText);
  });
  return control;
}

inline NoiseBandSliderControl* CreateNoiseAttackSliderControl(const std::shared_ptr<EditorContext>& context,
                                                              const EditorStyles& styles)
{
  auto* control = new NoiseBandSliderControl(IRECT(), "", styles.sliderStyle, EDirection::Vertical);
  NoiseBandSliderControl::Config config;
  config.range = {0.0, 1.0};
  config.transform = GetNoiseBandSliderValueTransform(*context->noiseAttackTab.transform);
  control->SetConfig(config);
  control->SetBandEditModeFunc([context]() {
    return *context->noiseAttackTab.editMode;
  });
  control->SetOnBandValueChanged([context, control](int bandIndex, double value) {
    if(bandIndex < 0 || bandIndex >= NoiseBandProfile::kNumBands)
      return;

    const int midiNote = context->SelectedMidiNote();
    const NoiseBandProfile* keyNoteProfile = context->Preset().GetKeyNoteNoiseAttackProfile(midiNote);
    if(!keyNoteProfile)
      return;

    NoiseBandProfile updatedProfile = *keyNoteProfile;
    updatedProfile.SetBandValue(bandIndex, value);
    if(!context->Preset().SetKeyNoteNoiseAttackProfile(midiNote, updatedProfile))
      return;

    context->SendNoiseAttackProfileToDSP(control, midiNote, updatedProfile);
  });
  return control;
}

inline void RestoreNoiseAttackValues(const std::shared_ptr<EditorContext>& context, IControl* caller)
{
  if(!caller || !context->HasValidSelectedMidiNote())
    return;

  const int midiNote = context->SelectedMidiNote();
  if(!context->Preset().HasKeyNotePreset(midiNote))
    return;

  auto* control = context->noiseAttackTab.sliderControl ? *context->noiseAttackTab.sliderControl : nullptr;
  if(!control || !control->HasRestoreStateForMidiNote(midiNote))
    return;

  NoiseBandProfile::BandValues values{};
  const auto& restoreState = control->GetRestoreState();
  for(int bandIndex = 0; bandIndex < NoiseBandProfile::kNumBands; ++bandIndex)
  {
    values[static_cast<std::size_t>(bandIndex)] =
      NoiseBandProfile::ClampBandValue(restoreState[static_cast<std::size_t>(bandIndex)]);
  }

  NoiseBandProfile profile{values};
  if(!context->Preset().SetKeyNoteNoiseAttackProfile(midiNote, profile))
    return;

  context->SendNoiseAttackProfileToDSP(control, midiNote, profile);
  context->RefreshOscillatorTabs();
}

inline IVTabPage* CreateNoiseAttackTabPage(const std::shared_ptr<EditorContext>& context,
                                           const EditorStyles& styles)
{
  return new EditorOscillatorTabPage(
    [context, styles](IVTabPage* page, const IRECT&) {
      const auto xRangeControls = CreateNoiseXRangeControls(context, styles);
      auto* sliderControl = CreateNoiseAttackSliderControl(context, styles);
      auto* yTransformControl = CreateNoiseAttackYTransformControl(context->noiseAttackTab.transform, sliderControl, styles);
      auto* editModeControl = CreateNoiseAttackEditModeControl(context->noiseAttackTab.editMode, styles);

      auto* setShapeControl = new ActionSelectionControl(
        IRECT(),
        "choose shape",
        {"flat", "tilt down", "tilt up"},
        styles.utilityDropdownText,
        styles.darkTab);
      setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
        if(!selectedText)
          return;

        context->ApplyNoiseAttackActionToSelectedKeyNote(
          sliderControl,
          [selectedText](NoiseBandProfile& profile) {
            return TryApplyNoiseAttackShape(profile, selectedText);
          });
      });

      auto* actionsControl = new ActionSelectionControl(
        IRECT(),
        "run action",
        {
          kActionScaleUpMenuLabel,
          kActionScaleDownMenuLabel,
          kActionTowardMaxMenuLabel,
          kActionAwayFromMaxMenuLabel,
          kActionBendUpMenuLabel,
          kActionBendDownMenuLabel,
          kActionNormalizeMenuLabel},
        styles.utilityDropdownText,
        styles.darkTab);
      actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
        if(!selectedText)
          return;

        context->ApplyNoiseAttackActionToSelectedKeyNote(
          sliderControl,
          [selectedText](NoiseBandProfile& profile) {
            return ApplyNoiseAttackAction(profile, selectedText);
          });
      });

      auto* allKeyNotesToggle = new IVToggleControl(
        IRECT(),
        [context](IControl* caller) {
          auto* toggle = caller ? caller->As<IVToggleControl>() : nullptr;
          if(!toggle || !context->HasValidSelectedMidiNote())
            return;

          const int midiNote = context->SelectedMidiNote();
          const NoiseBandProfile* keyNoteProfile = context->Preset().GetKeyNoteNoiseAttackProfile(midiNote);
          if(!keyNoteProfile)
          {
            SetControlValueSilently(toggle, context->IsAllKeyNotesNoiseAttackEnabled() ? 1.0 : 0.0);
            return;
          }

          if(toggle->GetValue() > 0.5)
          {
            context->Preset().EnableAllKeyNotesNoiseAttack(*keyNoteProfile);
            context->SendAllKeyNotesNoiseAttackEnabledToDSP(toggle, true);
            context->SendNoiseAttackProfileToDSP(toggle, midiNote, *keyNoteProfile);
          }
          else
          {
            context->Preset().SetAllKeyNotesNoiseAttackEnabled(false);
            context->SendAllKeyNotesNoiseAttackEnabledToDSP(toggle, false);
          }

          context->RefreshOscillatorTabs();
        },
        "",
        styles.utilityToggleStyle,
        "",
        "X",
        context->IsAllKeyNotesNoiseAttackEnabled());
      allKeyNotesToggle->SetTooltip(help_text::oscillator_tabs::kAllKeyNotes);

      auto* allKeyNotesLabel = new ITextControl(IRECT(), "All notes", styles.utilityLabelText, COLOR_TRANSPARENT);
      allKeyNotesLabel->SetIgnoreMouse(false);
      allKeyNotesLabel->SetTooltip(help_text::oscillator_tabs::kAllKeyNotes);
      allKeyNotesLabel->DisablePrompt(true);

      auto* restoreButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Restore", styles.restoreButtonStyle, true, false);
      restoreButton->SetAnimationEndActionFunction([context](IControl* caller) {
        RestoreNoiseAttackValues(context, caller);
      });

      const auto keyNoteActionButtons = CreateKeyNoteActionButtons(context, styles);

      *context->noiseAttackTab.xRangeMinControl = xRangeControls.minControl;
      *context->noiseAttackTab.xRangeMaxControl = xRangeControls.maxControl;
      *context->noiseAttackTab.sliderControl = sliderControl;
      *context->noiseAttackTab.setShapeControl = setShapeControl;
      *context->noiseAttackTab.actionsControl = actionsControl;
      *context->noiseAttackTab.allKeyNotesToggle = allKeyNotesToggle;
      *context->noiseAttackTab.restoreButton = restoreButton;
      *context->noiseAttackTab.addButton = keyNoteActionButtons.addButton;
      *context->noiseAttackTab.deleteButton = keyNoteActionButtons.deleteButton;

      page->AddChildControl(CreateUtilityLabelControl("X range:", styles));
      page->AddChildControl(xRangeControls.minControl);
      page->AddChildControl(xRangeControls.maxControl);
      page->AddChildControl(CreateUtilityLabelControl("Y transform:", styles));
      page->AddChildControl(yTransformControl);
      page->AddChildControl(CreateUtilityLabelControl("Edit mode:", styles));
      page->AddChildControl(editModeControl);
      page->AddChildControl(CreateUtilityLabelControl("Set shape:", styles));
      page->AddChildControl(setShapeControl);
      page->AddChildControl(CreateUtilityLabelControl("Actions:", styles));
      page->AddChildControl(actionsControl);
      page->AddChildControl(allKeyNotesToggle);
      page->AddChildControl(allKeyNotesLabel);
      page->AddChildControl(restoreButton);
      page->AddChildControl(keyNoteActionButtons.addButton);
      page->AddChildControl(keyNoteActionButtons.deleteButton);
      page->AddChildControl(sliderControl);

      context->RefreshOscillatorTabs();
    },
    ResizeNoiseSustainTabPage,
    [context](bool isVisible) {
      auto* control = context->noiseAttackTab.sliderControl ? *context->noiseAttackTab.sliderControl : nullptr;
      if(!control)
        return;

      if(isVisible)
      {
        if(context->HasValidSelectedMidiNote())
          control->CaptureRestoreState(context->SelectedMidiNote());
        else
          control->ClearRestoreState();
      }
      else
        control->ClearRestoreState();

      context->RefreshOscillatorTabs();
    });
}
} // namespace editor

#pragma once

#include "common.h"

namespace editor
{
inline NoiseBandSliderControl::ValueTransform GetNoiseBandSliderValueTransform(EditorLevelTransform transform)
{
  switch(transform)
  {
    case EditorLevelTransform::SquareRoot:
      return NoiseBandSliderControl::ValueTransform::SquareRoot;
    case EditorLevelTransform::PseudoLog:
      return NoiseBandSliderControl::ValueTransform::PseudoLog;
    case EditorLevelTransform::Linear:
    default:
      return NoiseBandSliderControl::ValueTransform::Linear;
  }
}

inline bool TryApplyNoiseSustainShape(NoiseBandProfile& profile, const char* shapeName)
{
  if(!shapeName)
    return false;

  NoiseBandProfile::BandValues values{};
  for(int bandIndex = 0; bandIndex < NoiseBandProfile::kNumBands; ++bandIndex)
  {
    const double t = NoiseBandProfile::kNumBands > 1
      ? static_cast<double>(bandIndex) / static_cast<double>(NoiseBandProfile::kNumBands - 1)
      : 0.0;

    if(std::strcmp(shapeName, "flat") == 0)
      values[static_cast<std::size_t>(bandIndex)] = 1.0;
    else if(std::strcmp(shapeName, "tilt down") == 0)
      values[static_cast<std::size_t>(bandIndex)] = 1.0 - t;
    else if(std::strcmp(shapeName, "tilt up") == 0)
      values[static_cast<std::size_t>(bandIndex)] = t;
    else
      return false;
  }

  profile.SetValues(values);
  return true;
}

inline bool ApplyNoiseSustainAction(NoiseBandProfile& profile, const char* actionName)
{
  const bool isScaleUp = MatchesActionLabel(actionName, kActionScaleUp);
  const bool isScaleDown = MatchesActionLabel(actionName, kActionScaleDown);
  const bool isTowardMax = MatchesActionLabel(actionName, kActionTowardMax);
  const bool isAwayFromMax = MatchesActionLabel(actionName, kActionAwayFromMax);
  const bool isBendUp = MatchesActionLabel(actionName, kActionBendUp);
  const bool isBendDown = MatchesActionLabel(actionName, kActionBendDown);
  const bool isNormalize = MatchesActionLabel(actionName, kActionNormalize);

  if(!(isScaleUp || isScaleDown || isTowardMax || isAwayFromMax || isBendUp || isBendDown || isNormalize))
    return false;

  constexpr double kScaleUp = 1.01010101010101;
  constexpr double kScaleDown = 0.99;
  constexpr double kTowardMaxFactor = 0.999;
  constexpr double kAwayFromMaxFactor = 1.001001001001001;
  constexpr double kBendExponent = 1.05;

  auto values = profile.GetValues();
  if(isNormalize)
  {
    double maxValue = 0.0;
    for(const double value : values)
      maxValue = std::max(maxValue, value);

    if(maxValue <= 1.0e-12)
      return false;

    for(double& value : values)
      value = std::clamp(value / maxValue, 0.0, 1.0);

    profile.SetValues(values);
    return true;
  }

  double currentMinValue = 1.0;
  double currentMaxValue = 0.0;
  if(isBendUp || isBendDown)
  {
    for(const double value : values)
    {
      currentMinValue = std::min(currentMinValue, value);
      currentMaxValue = std::max(currentMaxValue, value);
    }
  }

  for(double& value : values)
  {
    if(isScaleUp)
      value = ApplyLinearScale(value, kScaleUp, 0.0, 1.0);
    else if(isScaleDown)
      value = ApplyLinearScale(value, kScaleDown, 0.0, 1.0);
    else if(isTowardMax)
      value = ApplyTowardMaxScale(value, kTowardMaxFactor, 0.0, 1.0);
    else if(isAwayFromMax)
      value = ApplyTowardMaxScale(value, kAwayFromMaxFactor, 0.0, 1.0);
    else if(isBendUp)
      value = ApplyCurveWarp(value, 1.0 / kBendExponent, currentMinValue, currentMaxValue);
    else if(isBendDown)
      value = ApplyCurveWarp(value, kBendExponent, currentMinValue, currentMaxValue);
  }

  profile.SetValues(values);
  return true;
}

inline XRangeControls CreateNoiseXRangeControls(const std::shared_ptr<EditorContext>& context,
                                                const EditorStyles& styles)
{
  auto* minControl = new IVNumberBoxControl(
    IRECT(), kNoParameter, nullptr, "", styles.utilityNumberBoxStyle, false,
    static_cast<double>(context->NoiseXRangeMin()), 1.0, static_cast<double>(NoiseBandProfile::kNumBands), "%0.0f", false);
  auto* maxControl = new IVNumberBoxControl(
    IRECT(), kNoParameter, nullptr, "", styles.utilityNumberBoxStyle, false,
    static_cast<double>(context->NoiseXRangeMax()), 1.0, static_cast<double>(NoiseBandProfile::kNumBands), "%0.0f", false);
  minControl->SetDrawTriangle(false);
  maxControl->SetDrawTriangle(false);
  minControl->SetTooltip(help_text::oscillator_tabs::kNoiseXRangeMin);
  maxControl->SetTooltip(help_text::oscillator_tabs::kNoiseXRangeMax);

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
    context->SetNoiseXRange(RoundNoiseBandRangeValue(minControl->GetRealValue()),
                            RoundNoiseBandRangeValue(maxControl->GetRealValue()));
    context->ApplyVisibleNoiseRangeToSliders();
    context->SyncNoiseXRangeNumberBoxes();
  };

  minControl->SetActionFunction([clampEditedControl, updateVisibleRange, minControl](IControl*) {
    clampEditedControl(minControl);
    updateVisibleRange();
  });
  maxControl->SetActionFunction([clampEditedControl, updateVisibleRange, maxControl](IControl*) {
    clampEditedControl(maxControl);
    updateVisibleRange();
  });

  context->SyncNoiseXRangeNumberBoxes();
  return {minControl, maxControl};
}

inline ActionSelectionControl* CreateNoiseSustainYTransformControl(const std::shared_ptr<EditorLevelTransform>& transform,
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

inline ActionSelectionControl* CreateNoiseSustainEditModeControl(const std::shared_ptr<EditorOscillatorEditMode>& editMode,
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

inline NoiseBandSliderControl* CreateNoiseSustainSliderControl(const std::shared_ptr<EditorContext>& context,
                                                               const EditorStyles& styles)
{
  auto* control = new NoiseBandSliderControl(IRECT(), "", styles.sliderStyle, EDirection::Vertical);
  NoiseBandSliderControl::Config config;
  config.range = {0.0, 1.0};
  config.transform = GetNoiseBandSliderValueTransform(*context->noiseSustainTab.transform);
  control->SetConfig(config);
  control->SetBandEditModeFunc([context]() {
    return *context->noiseSustainTab.editMode;
  });
  control->SetOnBandValueChanged([context, control](int bandIndex, double value) {
    if(bandIndex < 0 || bandIndex >= NoiseBandProfile::kNumBands)
      return;

    const int midiNote = context->SelectedMidiNote();
    const NoiseBandProfile* keyNoteProfile = context->Preset().GetKeyNoteNoiseSustainProfile(midiNote);
    if(!keyNoteProfile)
      return;

    NoiseBandProfile updatedProfile = *keyNoteProfile;
    updatedProfile.SetBandValue(bandIndex, value);
    if(!context->Preset().SetKeyNoteNoiseSustainProfile(midiNote, updatedProfile))
      return;

    context->SendNoiseSustainProfileToDSP(control, midiNote, updatedProfile);
  });
  return control;
}

inline void RestoreNoiseSustainValues(const std::shared_ptr<EditorContext>& context, IControl* caller)
{
  if(!caller || !context->HasValidSelectedMidiNote())
    return;

  const int midiNote = context->SelectedMidiNote();
  if(!context->Preset().HasKeyNotePreset(midiNote))
    return;

  auto* control = context->noiseSustainTab.sliderControl ? *context->noiseSustainTab.sliderControl : nullptr;
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
  if(!context->Preset().SetKeyNoteNoiseSustainProfile(midiNote, profile))
    return;

  context->SendNoiseSustainProfileToDSP(control, midiNote, profile);
  context->RefreshOscillatorTabs();
}

inline void ResizeNoiseSustainTabPage(IContainerBase* pTab, const IRECT& r)
{
  if(pTab->NChildren() < 17)
    return;

  constexpr float kLeftInset = 104.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = kEditorControlHeight;
  constexpr float kButtonHeight = kEditorControlHeight;
  constexpr float kBottomPad = 8.f;
  constexpr float kTightGap = 0.f;
  constexpr float kGap = 10.f;
  constexpr float kHalfGap = 6.f;
  constexpr float kToggleLabelGap = 8.f;

  auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
  auto leftColumnBounds = innerBounds.GetFromLeft(kLeftInset);
  const float rowL = leftColumnBounds.L + 8.f;
  const float rowR = leftColumnBounds.R - 8.f;
  const float rowMid = (rowL + rowR) * 0.5f;
  const float buttonRowBottom = leftColumnBounds.B - kBottomPad;
  const float buttonRowTop = buttonRowBottom - kButtonHeight;
  auto addButtonBounds = IRECT(rowL, buttonRowTop, rowMid - kTabButtonHalfGap, buttonRowBottom);
  auto deleteButtonBounds = IRECT(rowMid + kTabButtonHalfGap, buttonRowTop, rowR, buttonRowBottom);
  const float restoreTop = addButtonBounds.T - kGap - kButtonHeight;
  auto restoreButtonBounds = IRECT(rowL, restoreTop, rowR, restoreTop + kButtonHeight);
  const float allKeyNotesTop = restoreButtonBounds.T - kGap - kControlHeight;
  auto allKeyNotesToggleBounds = IRECT(rowL, allKeyNotesTop, rowL + kControlHeight, allKeyNotesTop + kControlHeight);
  auto allKeyNotesLabelBounds = IRECT(
    allKeyNotesToggleBounds.R + kToggleLabelGap,
    allKeyNotesToggleBounds.T,
    rowR,
    allKeyNotesToggleBounds.B);
  const float actionsTop = allKeyNotesToggleBounds.T - kGap - kControlHeight;
  auto actionsBounds = IRECT(rowL, actionsTop, rowR, actionsTop + kControlHeight);
  auto actionsLabelBounds = IRECT(rowL, actionsBounds.T - kTightGap - kLabelHeight, rowR, actionsBounds.T - kTightGap);
  const float setShapeTop = actionsLabelBounds.T - kGap - kControlHeight;
  auto setShapeBounds = IRECT(rowL, setShapeTop, rowR, setShapeTop + kControlHeight);
  auto setShapeLabelBounds = IRECT(rowL, setShapeBounds.T - kTightGap - kLabelHeight, rowR, setShapeBounds.T - kTightGap);
  const float editModeTop = setShapeLabelBounds.T - kGap - kControlHeight;
  auto editModeBounds = IRECT(rowL, editModeTop, rowR, editModeTop + kControlHeight);
  auto editModeLabelBounds = IRECT(rowL, editModeBounds.T - kTightGap - kLabelHeight, rowR, editModeBounds.T - kTightGap);
  const float yTransformTop = editModeLabelBounds.T - kGap - kControlHeight;
  auto yTransformBounds = IRECT(rowL, yTransformTop, rowR, yTransformTop + kControlHeight);
  auto yTransformLabelBounds = IRECT(rowL, yTransformBounds.T - kTightGap - kLabelHeight, rowR, yTransformBounds.T - kTightGap);
  const float xRangeTop = yTransformLabelBounds.T - kGap - kControlHeight;
  auto xRangeMinBounds = IRECT(rowL, xRangeTop, rowMid - kHalfGap * 0.5f, xRangeTop + kControlHeight);
  auto xRangeMaxBounds = IRECT(rowMid + kHalfGap * 0.5f, xRangeMinBounds.T, rowR, xRangeMinBounds.B);
  auto xRangeLabelBounds = IRECT(rowL, xRangeMinBounds.T - kTightGap - kLabelHeight, rowR, xRangeMinBounds.T - kTightGap);
  const auto sliderBounds = GetOscillatorSliderBounds(pTab, r, kLeftInset);

  pTab->GetChild(0)->SetTargetAndDrawRECTs(xRangeLabelBounds);
  pTab->GetChild(1)->SetTargetAndDrawRECTs(xRangeMinBounds);
  pTab->GetChild(2)->SetTargetAndDrawRECTs(xRangeMaxBounds);
  pTab->GetChild(3)->SetTargetAndDrawRECTs(yTransformLabelBounds);
  pTab->GetChild(4)->SetTargetAndDrawRECTs(yTransformBounds);
  pTab->GetChild(5)->SetTargetAndDrawRECTs(editModeLabelBounds);
  pTab->GetChild(6)->SetTargetAndDrawRECTs(editModeBounds);
  pTab->GetChild(7)->SetTargetAndDrawRECTs(setShapeLabelBounds);
  pTab->GetChild(8)->SetTargetAndDrawRECTs(setShapeBounds);
  pTab->GetChild(9)->SetTargetAndDrawRECTs(actionsLabelBounds);
  pTab->GetChild(10)->SetTargetAndDrawRECTs(actionsBounds);
  pTab->GetChild(11)->SetTargetAndDrawRECTs(allKeyNotesToggleBounds);
  pTab->GetChild(12)->SetTargetAndDrawRECTs(allKeyNotesLabelBounds);
  pTab->GetChild(13)->SetTargetAndDrawRECTs(restoreButtonBounds);
  pTab->GetChild(14)->SetTargetAndDrawRECTs(addButtonBounds);
  pTab->GetChild(15)->SetTargetAndDrawRECTs(deleteButtonBounds);
  pTab->GetChild(16)->SetTargetAndDrawRECTs(sliderBounds);
}

inline IVTabPage* CreateNoiseSustainTabPage(const std::shared_ptr<EditorContext>& context,
                                            const EditorStyles& styles)
{
  return new EditorOscillatorTabPage(
    [context, styles](IVTabPage* page, const IRECT&) {
      const auto xRangeControls = CreateNoiseXRangeControls(context, styles);
      auto* sliderControl = CreateNoiseSustainSliderControl(context, styles);
      auto* yTransformControl = CreateNoiseSustainYTransformControl(context->noiseSustainTab.transform, sliderControl, styles);
      auto* editModeControl = CreateNoiseSustainEditModeControl(context->noiseSustainTab.editMode, styles);

      auto* setShapeControl = new ActionSelectionControl(
        IRECT(),
        "choose shape",
        {"flat", "tilt down", "tilt up"},
        styles.utilityDropdownText,
        styles.darkTab);
      setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
        if(!selectedText)
          return;

        context->ApplyNoiseSustainActionToSelectedKeyNote(
          sliderControl,
          [selectedText](NoiseBandProfile& profile) {
            return TryApplyNoiseSustainShape(profile, selectedText);
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

        context->ApplyNoiseSustainActionToSelectedKeyNote(
          sliderControl,
          [selectedText](NoiseBandProfile& profile) {
            return ApplyNoiseSustainAction(profile, selectedText);
          });
      });

      auto* allKeyNotesToggle = new IVToggleControl(
        IRECT(),
        [context](IControl* caller) {
          auto* toggle = caller ? caller->As<IVToggleControl>() : nullptr;
          if(!toggle || !context->HasValidSelectedMidiNote())
            return;

          const int midiNote = context->SelectedMidiNote();
          const NoiseBandProfile* keyNoteProfile = context->Preset().GetKeyNoteNoiseSustainProfile(midiNote);
          if(!keyNoteProfile)
          {
            SetControlValueSilently(toggle, context->IsAllKeyNotesNoiseSustainEnabled() ? 1.0 : 0.0);
            return;
          }

          if(toggle->GetValue() > 0.5)
          {
            context->Preset().EnableAllKeyNotesNoiseSustain(*keyNoteProfile);
            context->SendAllKeyNotesNoiseSustainEnabledToDSP(toggle, true);
            context->SendNoiseSustainProfileToDSP(toggle, midiNote, *keyNoteProfile);
          }
          else
          {
            context->Preset().SetAllKeyNotesNoiseSustainEnabled(false);
            context->SendAllKeyNotesNoiseSustainEnabledToDSP(toggle, false);
          }

          context->RefreshOscillatorTabs();
        },
        "",
        styles.utilityToggleStyle,
        "",
        "X",
        context->IsAllKeyNotesNoiseSustainEnabled());
      allKeyNotesToggle->SetTooltip(help_text::oscillator_tabs::kAllKeyNotes);

      auto* allKeyNotesLabel = new ITextControl(IRECT(), "All notes", styles.utilityLabelText, COLOR_TRANSPARENT);
      allKeyNotesLabel->SetIgnoreMouse(false);
      allKeyNotesLabel->SetTooltip(help_text::oscillator_tabs::kAllKeyNotes);
      allKeyNotesLabel->DisablePrompt(true);

      auto* restoreButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Restore", styles.restoreButtonStyle, true, false);
      restoreButton->SetAnimationEndActionFunction([context](IControl* caller) {
        RestoreNoiseSustainValues(context, caller);
      });

      const auto keyNoteActionButtons = CreateKeyNoteActionButtons(context, styles);

      *context->noiseSustainTab.xRangeMinControl = xRangeControls.minControl;
      *context->noiseSustainTab.xRangeMaxControl = xRangeControls.maxControl;
      *context->noiseSustainTab.sliderControl = sliderControl;
      *context->noiseSustainTab.setShapeControl = setShapeControl;
      *context->noiseSustainTab.actionsControl = actionsControl;
      *context->noiseSustainTab.allKeyNotesToggle = allKeyNotesToggle;
      *context->noiseSustainTab.restoreButton = restoreButton;
      *context->noiseSustainTab.addButton = keyNoteActionButtons.addButton;
      *context->noiseSustainTab.deleteButton = keyNoteActionButtons.deleteButton;

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
      auto* control = context->noiseSustainTab.sliderControl ? *context->noiseSustainTab.sliderControl : nullptr;
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

#pragma once

#include "common.h"
#include "eq.h"

#include <array>
#include <optional>
#include <utility>

namespace editor {
inline IVTabSwitchControl* CreateBreathCurveSwitchControl(ResonanceEditorControl* editorControl, const EditorStyles& styles) {
  auto* control = new EditorModeSwitchControl(
      IRECT(),
      [editorControl](IControl* caller) {
        editorControl->SetActiveBreath(caller->GetValue() < 0.5 ? ResonanceCurves::Breath::zero : ResonanceCurves::Breath::full);
      },
      {"Zero", "Full"}, "", styles.utilityToggleStyle.WithValueText(styles.utilityActionTitleText), EVShape::Rectangle, EDirection::Horizontal);
  control->SetTooltip(help_text::oscillator_tabs::kResonanceBreathCurve);
  return control;
}

inline void SetSelectedKeyNoteResonance(const std::shared_ptr<EditorContext>& context, IControl* caller, const ResonanceSettings& resonance) {
  if (!context->HasValidSelectedMidiNote()) return;

  const int midiNote = context->SelectedMidiNote();
  {
    const auto patchLock = context->LockPatch();
    if (!context->Patch().SetKeyNoteResonance(midiNote, resonance)) return;
  }
  context->SendResonanceToDSP(caller, midiNote, resonance);
}

inline std::optional<ResonanceSettings> GetSelectedKeyNoteResonance(const std::shared_ptr<EditorContext>& context) {
  if (!context->HasValidSelectedMidiNote()) return std::nullopt;

  const auto patchLock = context->LockPatch();
  const auto* resonance = context->Patch().GetKeyNoteResonance(context->SelectedMidiNote());
  return resonance ? std::optional{*resonance} : std::nullopt;
}

// Updates the editor directly rather than refreshing the tab, which would also reset the knob being dragged.
inline void SetSelectedKeyNoteResonanceValue(const std::shared_ptr<EditorContext>& context, const ResonanceKnob& knob) {
  auto resonance = GetSelectedKeyNoteResonance(context);
  if (!resonance) return;

  (*resonance).*(knob.member) = knob.FromNormalized(knob.control->GetNormalizedValue());
  SetSelectedKeyNoteResonance(context, knob.control, *resonance);
  context->resonanceTab.editorControl->SetBlendedResonance(*resonance, *resonance, 0.0);
}

// Edits the selected key note's curves; edit returns false if it changed nothing.
template <typename Edit> void EditSelectedKeyNoteResonanceCurves(const std::shared_ptr<EditorContext>& context, Edit&& edit) {
  auto resonance = GetSelectedKeyNoteResonance(context);
  if (!resonance || !std::forward<Edit>(edit)(resonance->curves)) return;

  SetSelectedKeyNoteResonance(context, context->resonanceTab.editorControl, *resonance);
  context->RefreshResonanceTab();
}

// Copy and swap act on both curves. The other actions act on the active curve only.
inline void ApplyResonanceActionToSelectedKeyNote(const std::shared_ptr<EditorContext>& context, const char* actionName) {
  const auto* editorControl = context->resonanceTab.editorControl;
  if (!editorControl) return;

  EditSelectedKeyNoteResonanceCurves(context, [actionName, activeBreath = editorControl->ActiveBreath()](ResonanceCurves& curves) {
    if (MatchesActionLabel(actionName, kActionCopyToOtherCurve)) {
      curves.CopyToOther(activeBreath);
      return true;
    }
    if (MatchesActionLabel(actionName, kActionSwapCurves)) {
      curves.Swap();
      return true;
    }
    const auto action = FindEqAction(actionName);
    return action && curves.ApplyAction(activeBreath, *action);
  });
}

inline const char* GetResonanceActionShortcutActionName(int keyVK) {
  switch (keyVK) {
  case kVK_C: return kActionCopyToOtherCurve;
  case kVK_X: return kActionSwapCurves;
  default:    return GetEqActionShortcutActionName(keyVK);
  }
}

// Enabling shares the selected key note's setup with every key note.
inline void SetAllKeyNotesResonanceEnabled(const std::shared_ptr<EditorContext>& context, IControl* toggle) {
  if (GetSelectedKeyNoteResonance(context)) {
    const int midiNote = context->SelectedMidiNote();
    const bool enable = toggle->GetValue() > 0.5;
    {
      const auto patchLock = context->LockPatch();
      context->Patch().SetAllKeyNotesResonanceEnabled(enable, midiNote);
    }
    context->SendAllKeyNotesResonanceEnabledToDSP(toggle, enable, midiNote);
  }
  context->RefreshResonanceTab();
}

inline void RestoreResonanceTab(const std::shared_ptr<EditorContext>& context, IControl* caller) {
  const auto& tab = context->resonanceTab;
  if (!context->HasValidSelectedMidiNote() || tab.restoreMidiNote != context->SelectedMidiNote()) return;

  SetSelectedKeyNoteResonance(context, caller, tab.restoreResonance);
  context->RefreshResonanceTab();
}

inline void ResizeResonanceTabPage(IContainerBase* pTab, const IRECT& r) {
  constexpr int kNumChildren = 13;
  if (pTab->NChildren() < kNumChildren) return;

  const auto layout = GetHarmonicTabLayout(pTab, r);
  const std::array<IRECT, kNumChildren> childBounds{{layout.modeToggleBounds, layout.addButtonBounds, layout.deleteButtonBounds, GetCurveEditorBounds(layout),
                                                     GetMacroKnobBounds(layout.macroAreaBounds, 0), GetMacroKnobBounds(layout.macroAreaBounds, 1),
                                                     layout.setShapeLabelBounds, layout.setShapeBounds, layout.actionsLabelBounds, layout.actionsBounds,
                                                     layout.allKeyNotesToggleBounds, layout.allKeyNotesLabelBounds, layout.restoreButtonBounds}};
  for (std::size_t i = 0; i < childBounds.size(); ++i) pTab->GetChild(static_cast<int>(i))->SetTargetAndDrawRECTs(childBounds[i]);
}

inline void AttachResonanceTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles) {
  auto* editorControl = new ResonanceEditorControl(IRECT());
  editorControl->SetTooltip(help_text::oscillator_tabs::kResonance);
  editorControl->SetOnResonanceChanged(
      [context, editorControl](const ResonanceSettings& resonance) { SetSelectedKeyNoteResonance(context, editorControl, resonance); });
  const auto keyNoteActionButtons = CreateKeyNoteActionButtons(context, styles);

  context->resonanceTab.editorControl = editorControl;
  context->resonanceTab.addButton = keyNoteActionButtons.addButton;
  context->resonanceTab.deleteButton = keyNoteActionButtons.deleteButton;

  page->AddChildControl(CreateBreathCurveSwitchControl(editorControl, styles));
  page->AddChildControl(keyNoteActionButtons.addButton);
  page->AddChildControl(keyNoteActionButtons.deleteButton);
  page->AddChildControl(editorControl);

  const std::array<std::pair<MacroKnobDescriptor, ResonanceKnob>, 2> knobs{{
      {{"Response", help_text::oscillator_tabs::kResonanceBreathResponse, 0.5, true}, {nullptr, &ResonanceSettings::breathResponse, -1.0, 1.0}},
      {{"Tracking", help_text::oscillator_tabs::kResonanceTracking, 0.0, false}, {nullptr, &ResonanceSettings::tracking, 0.0, 1.0}},
  }};
  for (std::size_t i = 0; i < knobs.size(); ++i) {
    auto& knob = context->resonanceTab.knobs[i] = knobs[i].second;
    knob.control = CreateMacroKnobControl(knobs[i].first, [context, i] { SetSelectedKeyNoteResonanceValue(context, context->resonanceTab.knobs[i]); });
    page->AddChildControl(knob.control);
  }

  // Every shape sets both curves, since they must have the same number of points. Flat is the only shape so far.
  auto* setShapeControl = new ActionSelectionControl(IRECT(), "choose shape", {"flat"}, styles.utilityDropdownText, styles.darkTab);
  setShapeControl->SetTooltip(help_text::oscillator_tabs::kResonanceSetShape);
  setShapeControl->SetOnSelection([context](const char*) {
    EditSelectedKeyNoteResonanceCurves(context, [](ResonanceCurves& curves) {
      curves = {};
      return true;
    });
  });

  auto* actionsControl = new ActionSelectionControl(IRECT(), "run action",
                                                    {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionShiftUpMenuLabel, kActionShiftDownMenuLabel,
                                                     kActionShiftRightMenuLabel, kActionShiftLeftMenuLabel, kActionNormalizeMenuLabel, kActionInvertMenuLabel,
                                                     kActionCopyToOtherCurveMenuLabel, kActionSwapCurvesMenuLabel},
                                                    styles.utilityDropdownText, styles.darkTab);
  actionsControl->SetTooltip(help_text::oscillator_tabs::kResonanceActions);
  actionsControl->SetOnSelection([context](const char* selectedText) { ApplyResonanceActionToSelectedKeyNote(context, selectedText); });

  // The toggle's value is set when the tab refreshes.
  const auto allKeyNotesControls = MakeAllKeyNotesControls(styles, [context](IControl* caller) { SetAllKeyNotesResonanceEnabled(context, caller); }, false);
  auto* restoreButton = CreateRestoreButton(styles, [context](IControl* caller) { RestoreResonanceTab(context, caller); });

  context->resonanceTab.setShapeControl = setShapeControl;
  context->resonanceTab.actionsControl = actionsControl;
  context->resonanceTab.allKeyNotesToggle = allKeyNotesControls.toggleControl;
  context->resonanceTab.restoreButton = restoreButton;

  page->AddChildControl(CreateUtilityLabelControl("Set shape:", styles, help_text::oscillator_tabs::kResonanceSetShape));
  page->AddChildControl(setShapeControl);
  page->AddChildControl(CreateUtilityLabelControl("Actions:", styles, help_text::oscillator_tabs::kResonanceActions));
  page->AddChildControl(actionsControl);
  page->AddChildControl(allKeyNotesControls.toggleControl);
  page->AddChildControl(allKeyNotesControls.labelControl);
  page->AddChildControl(restoreButton);
}

inline IVTabPage* CreateResonanceTabPage(const std::shared_ptr<EditorContext>& context, const EditorStyles& styles) {
  return new EditorOscillatorTabPage(
      [context, styles](IVTabPage* page, const IRECT&) {
        AttachResonanceTabChildren(page, context, styles);
        context->RefreshOscillatorTabs();
      },
      ResizeResonanceTabPage,
      [context](bool isVisible) {
        if (!isVisible) context->resonanceTab.restoreMidiNote = ResonanceTabControls::kNoRestoreMidiNote;
        context->RefreshResonanceTab();
      });
}
} // namespace editor

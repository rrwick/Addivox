#pragma once

#include "common.h"
#include "eq.h"

#include <array>
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

// Updates the editor directly rather than refreshing the tab, which would also reset the knob being dragged.
inline void SetSelectedKeyNoteResonanceValue(const std::shared_ptr<EditorContext>& context, const ResonanceKnob& knob) {
  ResonanceSettings resonance;
  {
    const auto patchLock = context->LockPatch();
    const auto* keyNoteResonance = context->Patch().GetKeyNoteResonance(context->SelectedMidiNote());
    if (!keyNoteResonance) return;
    resonance = *keyNoteResonance;
  }
  resonance.*(knob.member) = knob.FromNormalized(knob.control->GetNormalizedValue());
  SetSelectedKeyNoteResonance(context, knob.control, resonance);
  context->resonanceTab.editorControl->SetBlendedResonance(resonance, resonance, 0.0);
}

inline void ResizeResonanceTabPage(IContainerBase* pTab, const IRECT& r) {
  constexpr int kNumChildren = 6;
  if (pTab->NChildren() < kNumChildren) return;

  const auto layout = GetHarmonicTabLayout(pTab, r);
  const std::array<IRECT, kNumChildren> childBounds{{layout.modeToggleBounds, layout.addButtonBounds, layout.deleteButtonBounds, GetCurveEditorBounds(layout),
                                                     GetMacroKnobBounds(layout.macroAreaBounds, 0), GetMacroKnobBounds(layout.macroAreaBounds, 1)}};
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
}

inline IVTabPage* CreateResonanceTabPage(const std::shared_ptr<EditorContext>& context, const EditorStyles& styles) {
  return new EditorOscillatorTabPage(
      [context, styles](IVTabPage* page, const IRECT&) {
        AttachResonanceTabChildren(page, context, styles);
        context->RefreshOscillatorTabs();
      },
      ResizeResonanceTabPage, nullptr);
}
} // namespace editor

#pragma once

#include "common.h"
#include "eq.h"

#include <array>

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

inline void SetSelectedKeyNoteResonanceCurves(const std::shared_ptr<EditorContext>& context, IControl* caller, const ResonanceCurves& curves) {
  if (!context->HasValidSelectedMidiNote()) return;

  const int midiNote = context->SelectedMidiNote();
  {
    const auto patchLock = context->LockPatch();
    if (!context->Patch().SetKeyNoteResonanceCurves(midiNote, curves)) return;
  }
  context->SendResonanceCurvesToDSP(caller, midiNote, curves);
}

inline void ResizeResonanceTabPage(IContainerBase* pTab, const IRECT& r) {
  if (pTab->NChildren() < 4) return;

  const auto layout = GetHarmonicTabLayout(pTab, r);
  const std::array<IRECT, 4> childBounds{{layout.modeToggleBounds, layout.addButtonBounds, layout.deleteButtonBounds, GetCurveEditorBounds(layout)}};
  for (std::size_t i = 0; i < childBounds.size(); ++i) pTab->GetChild(static_cast<int>(i))->SetTargetAndDrawRECTs(childBounds[i]);
}

inline void AttachResonanceTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles) {
  auto* editorControl = new ResonanceEditorControl(IRECT());
  editorControl->SetTooltip(help_text::oscillator_tabs::kResonance);
  editorControl->SetOnCurvesChanged(
      [context, editorControl](const ResonanceCurves& curves) { SetSelectedKeyNoteResonanceCurves(context, editorControl, curves); });
  const auto keyNoteActionButtons = CreateKeyNoteActionButtons(context, styles);

  context->resonanceTab.editorControl = editorControl;
  context->resonanceTab.addButton = keyNoteActionButtons.addButton;
  context->resonanceTab.deleteButton = keyNoteActionButtons.deleteButton;

  page->AddChildControl(CreateBreathCurveSwitchControl(editorControl, styles));
  page->AddChildControl(keyNoteActionButtons.addButton);
  page->AddChildControl(keyNoteActionButtons.deleteButton);
  page->AddChildControl(editorControl);
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

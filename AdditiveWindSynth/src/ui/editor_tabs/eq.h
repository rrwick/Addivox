#pragma once

#include "common.h"

#include <string_view>

namespace editor
{
inline ITextControl* CreateEqTabTitleControl(const EditorStyles& styles)
{
  auto* control = new IMultiLineTextControl(IRECT(), "Formants EQ", styles.tabTitleText, COLOR_TRANSPARENT);
  control->SetIgnoreMouse(false);
  control->SetTooltip(help_text::oscillator_tabs::kEq);
  control->DisablePrompt(true);
  return control;
}

inline EqCurve MakeEqShapeCurve(const char* shapeName)
{
  const std::string_view shape = shapeName ? shapeName : "";
  if(shape.empty() || shape == "flat")
    return {};

  if(shape == "a")
  {
    return EqCurve{{
      {20.0, 0.0},
      {350.0, 6.0},
      {800.0, 14.0},
      {1400.0, -8.0},
      {2400.0, 7.0},
      {3600.0, -10.0},
      {20000.0, 0.0},
    }};
  }

  if(shape == "e")
  {
    return EqCurve{{
      {20.0, 0.0},
      {400.0, 5.0},
      {550.0, -5.0},
      {1800.0, 14.0},
      {2600.0, 8.0},
      {3800.0, 3.0},
      {20000.0, 0.0},
    }};
  }

  if(shape == "i")
  {
    return EqCurve{{
      {20.0, 0.0},
      {280.0, 4.0},
      {500.0, -8.0},
      {2300.0, 15.0},
      {3200.0, 10.0},
      {4200.0, 5.0},
      {20000.0, 0.0},
    }};
  }

  if(shape == "o")
  {
    return EqCurve{{
      {20.0, 0.0},
      {450.0, 10.0},
      {900.0, 7.0},
      {1700.0, -7.0},
      {2800.0, 3.0},
      {20000.0, 0.0},
    }};
  }

  if(shape == "u")
  {
    return EqCurve{{
      {20.0, 0.0},
      {325.0, 8.0},
      {700.0, 6.0},
      {1400.0, -6.0},
      {2500.0, -10.0},
      {20000.0, 0.0},
    }};
  }

  return {};
}

inline bool ApplyEqAction(EqCurve& curve, const char* actionName)
{
  const std::string_view action = actionName ? actionName : "";
  if(action.empty() || curve.Empty())
    return false;

  auto points = curve.GetPoints();
  if(action == "normalise")
  {
    double meanDb = 0.0;
    for(const auto& point : points)
      meanDb += point.gainDb;

    meanDb /= static_cast<double>(points.size());
    for(auto& point : points)
      point.gainDb -= meanDb;
  }
  else if(action == "scale up" || action == "scale down")
  {
    const double scale = (action == "scale up") ? 1.111111111111111 : 0.9;
    for(auto& point : points)
      point.gainDb *= scale;
  }
  else if(action == "shift right" || action == "shift left")
  {
    constexpr double kShiftRatio = 1.189207115002721; // Quarter-octave in log-frequency space.
    const double frequencyScale = (action == "shift right")
      ? kShiftRatio
      : (1.0 / kShiftRatio);

    for(auto& point : points)
      point.frequencyHz *= frequencyScale;
  }
  else
    return false;

  curve.SetPoints(std::move(points));
  return true;
}

inline void SetSelectedKeyNoteEqCurve(const std::shared_ptr<EditorContext>& context,
                                      IControl* caller,
                                      const EqCurve& curve,
                                      bool refreshOscillatorTabs)
{
  if(!caller || !context->HasValidSelectedMidiNote())
    return;

  const int midiNote = context->SelectedMidiNote();
  if(!context->Preset().SetKeyNoteEqCurve(midiNote, curve))
    return;

  context->SendEqCurveEditToDSP(caller, midiNote, curve);
  if(refreshOscillatorTabs)
    context->RefreshOscillatorTabs();
}

inline void ResizeEqTabPage(IContainerBase* pTab, const IRECT& r)
{
  if(pTab->NChildren() < 9)
    return;

  constexpr float kLeftInset = 104.f;
  constexpr float kColumnSideInset = 8.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = 24.f;
  constexpr float kButtonHeight = 24.f;
  constexpr float kGap = 8.f;
  constexpr float kTightGap = 4.f;
  constexpr float kBottomPad = 8.f;
  constexpr float kToggleLabelGap = 8.f;
  constexpr float kDescriptionHeight = 64.f;
  constexpr float kEditorGap = 15.f;

  auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
  auto leftColumnBounds = innerBounds.GetFromLeft(kLeftInset);
  auto editorBounds = GetOscillatorSliderBounds(pTab, r, kLeftInset);
  editorBounds.L += kEditorGap;

  const float restoreTop = leftColumnBounds.B - (kButtonHeight + kBottomPad);
  auto restoreButtonBounds = IRECT(
    leftColumnBounds.L + kColumnSideInset,
    restoreTop,
    leftColumnBounds.R - kColumnSideInset,
    leftColumnBounds.B - kBottomPad);

  const float rowL = leftColumnBounds.L + kColumnSideInset;
  const float rowR = leftColumnBounds.R - kColumnSideInset;
  auto titleBounds = IRECT(
    leftColumnBounds.L + 4.f,
    leftColumnBounds.T + 2.f,
    leftColumnBounds.R - 4.f,
    leftColumnBounds.T + 2.f + kDescriptionHeight);

  const float allKeyNotesTop = restoreButtonBounds.T - kGap - kControlHeight;
  auto allKeyNotesToggleBounds = IRECT(rowL, allKeyNotesTop, rowL + kControlHeight, allKeyNotesTop + kControlHeight);
  auto allKeyNotesLabelBounds = IRECT(
    allKeyNotesToggleBounds.R + kToggleLabelGap,
    allKeyNotesToggleBounds.T,
    rowR,
    allKeyNotesToggleBounds.B);
  const float actionsTop = allKeyNotesToggleBounds.T - kGap - kControlHeight;
  auto actionsBounds = IRECT(rowL, actionsTop, rowR, actionsTop + kControlHeight);
  auto actionsLabelBounds = IRECT(
    rowL,
    actionsBounds.T - kTightGap - kLabelHeight,
    rowR,
    actionsBounds.T - kTightGap);
  const float setShapeTop = actionsLabelBounds.T - kGap - kControlHeight;
  auto setShapeBounds = IRECT(rowL, setShapeTop, rowR, setShapeTop + kControlHeight);
  auto setShapeLabelBounds = IRECT(
    rowL,
    setShapeBounds.T - kTightGap - kLabelHeight,
    rowR,
    setShapeBounds.T - kTightGap);

  pTab->GetChild(0)->SetTargetAndDrawRECTs(titleBounds);
  pTab->GetChild(1)->SetTargetAndDrawRECTs(setShapeLabelBounds);
  pTab->GetChild(2)->SetTargetAndDrawRECTs(setShapeBounds);
  pTab->GetChild(3)->SetTargetAndDrawRECTs(actionsLabelBounds);
  pTab->GetChild(4)->SetTargetAndDrawRECTs(actionsBounds);
  pTab->GetChild(5)->SetTargetAndDrawRECTs(allKeyNotesToggleBounds);
  pTab->GetChild(6)->SetTargetAndDrawRECTs(allKeyNotesLabelBounds);
  pTab->GetChild(7)->SetTargetAndDrawRECTs(restoreButtonBounds);
  pTab->GetChild(8)->SetTargetAndDrawRECTs(editorBounds);
}

inline AllKeyNotesControls CreateEqAllKeyNotesControls(const std::shared_ptr<EditorContext>& context,
                                                       const EditorStyles& styles)
{
  auto* toggleControl = new IVToggleControl(
    IRECT(),
    [context](IControl* caller) {
      auto* toggle = caller ? caller->As<IVToggleControl>() : nullptr;
      if(!toggle || !context->HasValidSelectedMidiNote())
        return;

      const int midiNote = context->SelectedMidiNote();
      const EqCurve* keyNoteEqCurve = context->Preset().GetKeyNoteEqCurve(midiNote);
      if(!keyNoteEqCurve)
      {
        toggle->SetValue(context->IsAllKeyNotesEqEnabled() ? 1.0 : 0.0);
        toggle->SetDirty(false);
        return;
      }

      if(toggle->GetValue() > 0.5)
      {
        context->Preset().EnableAllKeyNotesEq(*keyNoteEqCurve);
        context->SendAllKeyNotesEqEnabledToDSP(toggle, true);
        context->SendEqCurveEditToDSP(toggle, midiNote, *keyNoteEqCurve);
      }
      else
      {
        context->Preset().SetAllKeyNotesEqEnabled(false);
        context->SendAllKeyNotesEqEnabledToDSP(toggle, false);
      }

      context->RefreshOscillatorTabs();
    },
    "",
    styles.utilityToggleStyle,
    "",
    "X",
    context->IsAllKeyNotesEqEnabled());
  toggleControl->SetTooltip(help_text::oscillator_tabs::kAllKeyNotes);

  auto* labelControl = new ITextControl(IRECT(), "All notes", styles.utilityLabelText, COLOR_TRANSPARENT);
  labelControl->SetIgnoreMouse(false);
  labelControl->SetTooltip(help_text::oscillator_tabs::kAllKeyNotes);
  labelControl->DisablePrompt(true);

  *context->eqTab.allKeyNotesToggle = toggleControl;
  return {toggleControl, labelControl};
}

inline void RestoreEqTabValues(const std::shared_ptr<EditorContext>& context, IControl* caller)
{
  if(!caller || !context->HasValidSelectedMidiNote())
    return;

  const int midiNote = context->SelectedMidiNote();
  if(!context->Preset().HasKeyNotePreset(midiNote))
    return;

  auto* control = context->eqTab.editorControl ? *context->eqTab.editorControl : nullptr;
  if(!control || !control->HasRestoreStateForMidiNote(midiNote))
    return;

  const EqCurve& restoreState = control->GetRestoreState();
  SetSelectedKeyNoteEqCurve(context, caller, restoreState, true);
}

inline EqEditorControl* CreateEqEditorControl(const std::shared_ptr<EditorContext>& context)
{
  auto* control = new EqEditorControl(IRECT());
  control->SetTooltip(help_text::oscillator_tabs::kEq);
  control->SetOnCurveChanged([context, control](const EqCurve& curve) {
    SetSelectedKeyNoteEqCurve(context, control, curve, false);
  });
  return control;
}

inline void AttachEqTabChildren(IVTabPage* page,
                                const std::shared_ptr<EditorContext>& context,
                                const EditorStyles& styles)
{
  const auto allKeyNotesControls = CreateEqAllKeyNotesControls(context, styles);

  auto* setShapeControl = new ActionSelectionControl(
    IRECT(),
    "choose shape",
    {"flat", "a", "e", "i", "o", "u"},
    styles.utilityDropdownText,
    styles.darkTab);
  auto* actionsControl = new ActionSelectionControl(
    IRECT(),
    "run action",
    {"normalise", "scale up", "scale down", "shift right", "shift left"},
    styles.utilityDropdownText,
    styles.darkTab);
  auto* editorControl = CreateEqEditorControl(context);
  auto* restoreButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Restore", styles.restoreButtonStyle, true, false);

  setShapeControl->SetOnSelection([context, editorControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyEqCurveActionToSelectedKeyNote(
      editorControl,
      [selectedText](EqCurve& curve) {
        curve = MakeEqShapeCurve(selectedText);
        return true;
      });
  });

  actionsControl->SetOnSelection([context, editorControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyEqCurveActionToSelectedKeyNote(
      editorControl,
      [selectedText](EqCurve& curve) {
        return ApplyEqAction(curve, selectedText);
      });
  });

  restoreButton->SetAnimationEndActionFunction([context](IControl* caller) {
    RestoreEqTabValues(context, caller);
  });

  *context->eqTab.setShapeControl = setShapeControl;
  *context->eqTab.actionsControl = actionsControl;
  *context->eqTab.restoreButton = restoreButton;
  *context->eqTab.editorControl = editorControl;

  page->AddChildControl(CreateEqTabTitleControl(styles));
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Set shape:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(setShapeControl);
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Actions:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(actionsControl);
  page->AddChildControl(allKeyNotesControls.toggleControl);
  page->AddChildControl(allKeyNotesControls.labelControl);
  page->AddChildControl(restoreButton);
  page->AddChildControl(editorControl);
}

inline IVTabPage* CreateEqTabPage(const std::shared_ptr<EditorContext>& context,
                                  const EditorStyles& styles)
{
  return new EditorOscillatorTabPage(
    [context, styles](IVTabPage* page, const IRECT&) {
      AttachEqTabChildren(page, context, styles);
      context->RefreshOscillatorTabs();
    },
    ResizeEqTabPage,
    [context](bool isVisible) {
      auto* control = context->eqTab.editorControl ? *context->eqTab.editorControl : nullptr;
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

#pragma once

#include "common.h"

#include <array>
#include <string_view>
#include <utility>

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

template <std::size_t N>
inline EqCurve MakeEqCurveFromPointArrays(const std::array<double, N>& frequenciesHz,
                                          const std::array<double, N>& gainsDb)
{
  EqCurve::PointList points;
  points.reserve(N);
  for(std::size_t i = 0; i < N; ++i)
    points.push_back({frequenciesHz[i], gainsDb[i]});

  return EqCurve{std::move(points)};
}

inline EqCurve MakeEqShapeCurve(const char* shapeName)
{
  const std::string_view shape = shapeName ? shapeName : "";

  if(shape.empty() || shape == "flat")
    return {};

  if(shape == "low-pass")
  {
    static constexpr std::array<double, 3> kFrequenciesHz{{1000.0, 2000.0, 3000.0}};
    static constexpr std::array<double, 3> kGainsDb{{         0.0,  -24.0, -160.0}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "high-pass")
  {
    static constexpr std::array<double, 3> kFrequenciesHz{{250.0, 500.0, 1000.0}};
    static constexpr std::array<double, 3> kGainsDb{{     -160.0, -24.0,    0.0}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "5 waves")
  {
    static constexpr std::array<double, 11> kFrequenciesHz{{20.0000, 40.2874, 81.1537, 163.474, 329.296, 663.325, 1336.18, 2691.57, 5421.81, 10921.5, 22000.0}};
    static constexpr std::array<double, 11> kGainsDb{{         10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "10 waves")
  {
    static constexpr std::array<double, 21> kFrequenciesHz{{20.0000, 28.3857, 40.2874, 57.1793, 81.1537, 115.180, 163.474, 232.016, 329.296, 467.366, 663.325, 941.447, 1336.18, 1896.42, 2691.57, 3820.10, 5421.81, 7695.09, 10921.5, 15500.8, 22000.0}};
    static constexpr std::array<double, 21> kGainsDb{{         10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "15 waves")
  {
    static constexpr std::array<double, 31> kFrequenciesHz{{20.0000, 25.2586, 31.8999, 40.2874, 50.8802, 64.2582, 81.1537, 102.492, 129.440, 163.474, 206.456, 260.740, 329.296, 415.879, 525.226, 663.325, 837.734, 1058.00, 1336.18, 1687.51, 2131.20, 2691.57, 3399.26, 4293.03, 5421.81, 6847.37, 8647.76, 10921.5, 13793.1, 17419.8, 22000.0}};
    static constexpr std::array<double, 31> kGainsDb{{         10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "20 waves")
  {
    static constexpr std::array<double, 41> kFrequenciesHz{{20.0000, 23.8268, 28.3857, 33.8170, 40.2874, 47.9959, 57.1793, 68.1199, 81.1537, 96.6815, 115.180, 137.219, 163.474, 194.752, 232.016, 276.409, 329.296, 392.303, 467.366, 556.790, 663.325, 790.244, 941.447, 1121.58, 1336.18, 1591.84, 1896.42, 2259.28, 2691.57, 3206.56, 3820.10, 4551.03, 5421.81, 6459.20, 7695.09, 9167.45, 10921.5, 13011.2, 15500.8, 18466.6, 22000.0}};
    static constexpr std::array<double, 41> kGainsDb{{         10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0,   -10.0,    10.0}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "5 peaks")
  {
    static constexpr std::array<double, 15> kFrequenciesHz{{31.8999, 40.2874, 50.8802, 129.440, 163.474, 206.456, 525.226, 663.325, 837.734, 2131.20, 2691.57, 3399.26, 8647.76, 10921.5, 13793.1}};
    static constexpr std::array<double, 15> kGainsDb{{        -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "10 peaks")
  {
    static constexpr std::array<double, 30> kFrequenciesHz{{25.0691, 31.4231, 39.3875, 49.3705, 61.8838, 77.5687, 97.2291, 121.872, 152.762, 191.480, 240.012, 300.845, 377.097, 472.675, 592.477, 742.645, 930.873, 1166.81, 1462.55, 1833.24, 2297.88, 2880.30, 3610.33, 4525.40, 5672.39, 7110.10, 8912.20, 11171.1, 14002.4, 17551.5}};
    static constexpr std::array<double, 30> kGainsDb{{        -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0,   -10.0,    10.0,   -10.0}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "ä vowel")
  {
    static constexpr std::array<double, 9> kFrequenciesHz{{           20.0, 526.609023693897,           730.0, 901.797456323916,          1090.0,           1650.0,         2440.0, 3566.545601742707,         22000.0}};
    static constexpr std::array<double, 9> kGainsDb{{      -4.962230274005,   0.013576984406, 19.357417626702,  10.525862216949, 21.477278533181, -16.259403431515, 5.157596762298,    -3.43771648407, -6.834614978987}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "o vowel")
  {
    static constexpr std::array<double, 11> kFrequenciesHz{{           20.0, 206.310017585657, 353.625193058002, 485.098808195888, 611.179486996529, 866.427649737949,           1450.0, 1986.169938908097,         2400.0, 3151.319868426438,         22000.0}};
    static constexpr std::array<double, 11> kGainsDb{{      -1.298474949376,    1.80323266983,           22.125,  10.261422157288,  18.763577699661,  -1.441809654236, -19.191716375988,   -9.171335220337, 3.958136847606,   -6.173274993896, -8.153263701091}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
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
    std::size_t activePointCount = 0;
    for(const auto& point : points)
    {
      if(EqCurve::IsMutedGainDb(point.gainDb))
        continue;
      meanDb += point.gainDb;
      ++activePointCount;
    }

    if(activePointCount == 0)
      return false;

    meanDb /= static_cast<double>(activePointCount);
    for(auto& point : points)
    {
      if(EqCurve::IsMutedGainDb(point.gainDb))
        continue;
      point.gainDb = EqCurve::ClampGainDb(point.gainDb - meanDb);
    }
  }
  else if(action == "invert")
  {
    for(auto& point : points)
    {
      if(EqCurve::IsMutedGainDb(point.gainDb))
        continue;
      point.gainDb = EqCurve::ClampGainDb(-point.gainDb);
    }
  }
  else if(action == "scale up" || action == "scale down")
  {
    const double scale = (action == "scale up") ? 1.111111111111111 : 0.9;
    for(auto& point : points)
    {
      if(EqCurve::IsMutedGainDb(point.gainDb))
        continue;
      point.gainDb = EqCurve::ClampGainDb(point.gainDb * scale);
    }
  }
  else if(action == "shift up" || action == "shift down")
  {
    const double gainOffsetDb = (action == "shift up") ? 1.0 : -1.0;
    for(auto& point : points)
    {
      if(EqCurve::IsMutedGainDb(point.gainDb))
        continue;
      point.gainDb = EqCurve::ClampGainDb(point.gainDb + gainOffsetDb);
    }
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

  context->SendEqCurveToDSP(caller, midiNote, curve);
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
        context->SendEqCurveToDSP(toggle, midiNote, *keyNoteEqCurve);
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
    {"flat", "low-pass", "high-pass", "5 waves", "10 waves", "15 waves", "20 waves", "5 peaks", "10 peaks", "ä vowel", "o vowel"},
    styles.utilityDropdownText,
    styles.darkTab);
  auto* actionsControl = new ActionSelectionControl(
    IRECT(),
    "run action",
    {"normalise", "invert", "scale up", "scale down", "shift up", "shift down", "shift right", "shift left"},
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

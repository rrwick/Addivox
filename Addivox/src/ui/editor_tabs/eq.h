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

  if(shape == "i vowel")
  {
    static constexpr std::array<double, 21> kFrequenciesHz{{            20.0,             99.0,           181.5,           250.0, 326.946753119458,           575.0,            950.0,           1312.5,           1325.0,           1650.0,          1987.5,         2120.0,          2300.0,         2480.0,         2940.0,          3200.0,         3460.0,         4160.0, 5526.73690428382,         22000.0}};
    static constexpr std::array<double, 21> kGainsDb{{      -10.669079408627, -10.627677183356, -7.653731138546, -0.790423715897,  -3.491380691528, -8.905270538028, -18.610932784636, -17.994479500076, -17.990288065844, -16.044532845603, -8.024493217497, 3.784863587868, 23.429210486206, 9.729722603262, 5.120969364426, 14.739050449627, 6.199210486206, -0.81755067825,  -2.268104553223, -3.044410912971}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "e vowel")
  {
    static constexpr std::array<double, 18> kFrequenciesHz{{           20.0,           176.0,          300.0,           390.0,          480.0,           737.5,           1150.0,           1562.5,         1930.0,          2150.0,         2370.0,        2690.0,        2950.0,         3210.0,          4480.0,          8400.0,         22000.0}};
    static constexpr std::array<double, 18> kGainsDb{{      -6.774016494776, -6.370438110722, 0.928027570323, 13.121990211519, 5.646713746211, -7.988411319413, -16.955760470118, -11.763774153669, 4.074336567935, 23.356431099595, 9.878782028485, 4.36175887822, 12.8480617792, 5.465266134905, -0.903810394757, -2.935451912818, -3.380912462531}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "ä vowel")
  {
    static constexpr std::array<double, 15> kFrequenciesHz{{           20.0,           220.0,          580.0,           730.0, 901.797456323916,          1090.0, 1308.408178799292,           1650.0,           2025.0,         2180.0,         2440.0,         2700.0,          9000.0,         22000.0}};
    static constexpr std::array<double, 15> kGainsDb{{      -4.962230274005, -4.021422169249, 4.928410455634, 21.357417626702,  10.525862216949, 23.477278533181,   -3.714440345764, -16.259403431515, -11.138957323668, -5.15167181126, 5.157596762298, 0.628089253468, -6.342189667428, -6.834614978987}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "o vowel")
  {
    static constexpr std::array<double, 16> kFrequenciesHz{{           20.0,            72.0, 206.310017585657,           360.0, 485.098808195888,           640.0,          760.0,          1000.0,           1450.0,           1900.0,          2160.0,         2400.0, 3286.297606332025,         10000.0,         22000.0}};
    static constexpr std::array<double, 16> kGainsDb{{      -1.298474949376, -1.293219805343,    1.80323266983, 16.355452348292,  10.261422157288, 22.113071003985, 8.345592265987, -9.174591547456, -19.191716375988, -13.200635443203, -5.131875498073, 3.958136847606,   -3.153233528137, -8.009878241557, -8.153263701091}};
    return MakeEqCurveFromPointArrays(kFrequenciesHz, kGainsDb);
  }

  if(shape == "u vowel")
  {
    static constexpr std::array<double, 16> kFrequenciesHz{{           20.0, 146.048935943401,          250.0,          330.0,          505.0,           595.0,          685.0,            900.0,         1350.0,           1800.0,          2180.0,         2400.0, 3124.442947283641,         10000.0,         22000.0}};
    static constexpr std::array<double, 16> kGainsDb{{      -2.743455418381,  -2.475646018982, 9.730978052126, 5.104769547325, 7.982293552812, 22.222556927298, 7.999001371742, -10.462100137174, -22.0289478738, -15.418170096022, -5.914683127572, 0.883015089163,   -6.576078414917, -9.493193415638, -9.599301783265}};
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
    {"flat", "i vowel", "e vowel", "ä vowel", "o vowel", "u vowel"},
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

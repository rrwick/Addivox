#pragma once

#include "IControls.h"
#include "IVPresetManagerControls.h"
#include "colour.h"
#include "control_utils.h"
#include "layered_svg_knob_control.h"
#include "editor_panel.h"
#include "theme.h"
#include "../editor_messages.h"
#include "../settings/params.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace layout
{
struct KnobAssets
{
  ISVG fixed;
  ISVG rotating;
};

struct KnobValueSpec
{
  IRECT knobBounds;
  IRECT valueBounds;
  int paramIdx = kNoParameter;
};

struct LabelledKnobValueSpec : KnobValueSpec
{
  const char* label = "";
  IRECT labelBounds;
};

inline void AttachKnob(IGraphics* pGraphics, const KnobAssets& assets, const IRECT& bounds, int paramIdx)
{
  pGraphics->AttachControl(new LayeredSVGKnobControl(bounds, assets.fixed, assets.rotating, paramIdx));
}

inline void AttachPassiveText(IGraphics* pGraphics, const IRECT& bounds, const char* text, const IText& style)
{
  pGraphics->AttachControl(MakePassiveControl(new ITextControl(bounds, text, style, COLOR_TRANSPARENT)));
}

inline void AttachPassiveCaption(IGraphics* pGraphics, const IRECT& bounds, int paramIdx, const IText& style)
{
  pGraphics->AttachControl(MakePassiveControl(new ICaptionControl(bounds, paramIdx, style, COLOR_TRANSPARENT, true)));
}

inline void AttachKnobWithValue(IGraphics* pGraphics,
                                const KnobAssets& assets,
                                const KnobValueSpec& spec,
                                const IText& valueText)
{
  AttachPassiveCaption(pGraphics, spec.valueBounds, spec.paramIdx, valueText);
  AttachKnob(pGraphics, assets, spec.knobBounds, spec.paramIdx);
}

inline void AttachLabelledKnobWithValue(IGraphics* pGraphics,
                                        const KnobAssets& assets,
                                        const LabelledKnobValueSpec& spec,
                                        const IText& labelText,
                                        const IText& valueText)
{
  AttachKnobWithValue(pGraphics, assets, spec, valueText);
  AttachPassiveText(pGraphics, spec.labelBounds, spec.label, labelText);
}

using OutMeterLEDRange = IVLEDMeterControl<2>::LEDRange;

inline std::vector<OutMeterLEDRange> MakeOutputMeterLEDRanges()
{
  constexpr int kNumBars = 26;
  constexpr float kLowDB = -73.5f;
  constexpr float kHighDB = 4.5f;
  constexpr float kStepDB = (kHighDB - kLowDB) / static_cast<float>(kNumBars);

  std::vector<OutMeterLEDRange> ranges;
  ranges.reserve(kNumBars);
  for(int i = 0; i < kNumBars; ++i)
  {
    const float lowDB = kLowDB + (kStepDB * static_cast<float>(i));
    ranges.emplace_back(lowDB, lowDB + kStepDB, 1, colour::visualizer::kOutputMeterLEDColors[static_cast<std::size_t>(i)]);
  }
  return ranges;
}

class BakedPresetManagerControl final : public IVBakedPresetManagerControl
{
public:
  using IVBakedPresetManagerControl::IVBakedPresetManagerControl;

  void SetPresetLabel(const char* label)
  {
    if(!mPresetNameButton)
      return;

    mPresetNameButton->SetLabelStr((label && label[0] != '\0') ? label : "Choose Preset...");
  }

  void OnAttached() override
  {
    const auto restorePreset = [this](IPluginBase* pluginBase, int presetIdx) {
      if(!pluginBase || pluginBase->NPresets() == 0)
        return;

      pluginBase->RestorePreset(presetIdx);
      UpdatePresetLabel(pluginBase);
    };

    const auto prevPresetFunc = [restorePreset](IControl* caller) {
      auto* pluginBase = dynamic_cast<IPluginBase*>(caller->GetDelegate());
      if(!pluginBase || pluginBase->NPresets() == 0)
        return;

      int presetIdx = pluginBase->GetCurrentPresetIdx() - 1;
      if(presetIdx < 0)
        presetIdx = pluginBase->NPresets() - 1;

      restorePreset(pluginBase, presetIdx);
    };

    const auto nextPresetFunc = [restorePreset](IControl* caller) {
      auto* pluginBase = dynamic_cast<IPluginBase*>(caller->GetDelegate());
      if(!pluginBase || pluginBase->NPresets() == 0)
        return;

      int presetIdx = pluginBase->GetCurrentPresetIdx() + 1;
      if(presetIdx >= pluginBase->NPresets())
        presetIdx = 0;

      restorePreset(pluginBase, presetIdx);
    };

    const auto choosePresetFunc = [this](IControl* caller) {
      mPresetMenu.Clear();

      auto* pluginBase = dynamic_cast<IPluginBase*>(caller->GetDelegate());
      if(!pluginBase)
        return;

      const int currentPresetIdx = pluginBase->GetCurrentPresetIdx();
      for(int i = 0; i < pluginBase->NPresets(); ++i)
      {
        const char* presetName = pluginBase->GetPresetName(i);
        if(i == currentPresetIdx)
          mPresetMenu.AddItem(presetName, -1, IPopupMenu::Item::kChecked);
        else
          mPresetMenu.AddItem(presetName);
      }

      caller->GetUI()->CreatePopupMenu(*this, mPresetMenu, caller->GetRECT());
    };

    AddChildControl(new IVButtonControl(IRECT(), SplashClickActionFunc, "<", mStyle))
      ->SetAnimationEndActionFunction(prevPresetFunc);
    AddChildControl(new IVButtonControl(IRECT(), SplashClickActionFunc, ">", mStyle))
      ->SetAnimationEndActionFunction(nextPresetFunc);
    AddChildControl(mPresetNameButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Choose Preset...", mStyle))
      ->SetAnimationEndActionFunction(choosePresetFunc);

    OnResize();
    UpdatePresetLabel(dynamic_cast<IPluginBase*>(GetDelegate()));
  }

  void OnPopupMenuSelection(IPopupMenu* selectedMenu, int valIdx) override
  {
    if(!selectedMenu)
      return;

    auto* selectedItem = selectedMenu->GetChosenItem();
    if(!selectedItem)
      return;

    auto* pluginBase = dynamic_cast<IPluginBase*>(GetDelegate());
    if(!pluginBase)
      return;

    pluginBase->RestorePreset(selectedMenu->GetChosenItemIdx());
    UpdatePresetLabel(pluginBase);
  }

  void OnResize() override
  {
    MakeRects(mRECT);

    ForAllChildrenFunc([&](int childIdx, IControl* child) {
      child->SetTargetAndDrawRECTs(GetSubControlBounds(static_cast<ESubControl>(childIdx)));
    });
  }

private:
  void UpdatePresetLabel(IPluginBase* pluginBase)
  {
    if(!pluginBase || !mPresetNameButton || pluginBase->NPresets() == 0)
      return;

    WDL_String label;
    const int presetIdx = pluginBase->GetCurrentPresetIdx();
    label.SetFormatted(32, "%s", pluginBase->GetPresetName(presetIdx));
    SetPresetLabel(label.Get());
  }

  IRECT GetSubControlBounds(ESubControl control) const
  {
    auto sections = mWidgetBounds;

    std::array<IRECT, 3> rects = {
      sections.ReduceFromLeft(50),
      sections.ReduceFromLeft(50),
      sections
    };

    return rects[static_cast<int>(control)];
  }

  IPopupMenu mPresetMenu;
  IVButtonControl* mPresetNameButton = nullptr;
};

inline void AttachTitlePanelControls(IGraphics* pGraphics,
                                     const std::shared_ptr<editor::EditorContext>& context)
{
  const auto sendPresetFileMessage = [](IControl* caller, int msgTag) {
    if(!caller)
      return;

    if(auto* delegate = caller->GetDelegate())
      delegate->SendArbitraryMsgFromUI(msgTag, caller->GetTag());
  };

  auto* loadPresetButton = new IVButtonControl(
    IRECT::MakeXYWH(348.f, 14.f, 80.f, 42.f),
    SplashClickActionFunc,
    "Load",
    theme::PresetActionButtonStyle(),
    true,
    false);
  loadPresetButton->SetAnimationEndActionFunction([sendPresetFileMessage](IControl* caller) {
    sendPresetFileMessage(caller, editor_messages::kMsgTagPromptLoadPresetFromFile);
  });

  auto* savePresetButton = new IVButtonControl(
    IRECT::MakeXYWH(436.f, 14.f, 80.f, 42.f),
    SplashClickActionFunc,
    "Save",
    theme::PresetActionButtonStyle(),
    true,
    false);
  savePresetButton->SetAnimationEndActionFunction([sendPresetFileMessage](IControl* caller) {
    sendPresetFileMessage(caller, editor_messages::kMsgTagPromptSavePresetToFile);
  });

  auto* presetManagerControl = new BakedPresetManagerControl(
    IRECT::MakeXYWH(528.f, 14.f, 250.f, 42.f),
    "",
    theme::PresetManagerStyle());

  pGraphics->AttachControl(loadPresetButton);
  pGraphics->AttachControl(savePresetButton);
  pGraphics->AttachControl(presetManagerControl);
  *context->title.presetManagerControl = presetManagerControl;
}

struct PanelResources
{
  explicit PanelResources(IGraphics* pGraphics)
  : knobAssets{pGraphics->LoadSVG("knob-fixed.svg"), pGraphics->LoadSVG("knob-rotating.svg")}
  {
  }

  float knobSize = 52.f;
  IVStyle meterStyle = theme::MeterStyle();
  IVStyle vizEditButtonStyle = theme::VizEditButtonStyle();
  IVStyle mainPanelModeSwitchStyle = theme::MainPanelModeSwitchStyle();
  IVStyle portamentoRangeSliderStyle = theme::PortamentoRangeSliderStyle();
  IText compactLabelText = theme::CompactLabelText();
  IText compactValueText = theme::CompactValueText();
  IText portamentoValueText = theme::PortamentoValueText();
  KnobAssets knobAssets;
};

inline PanelResources MakePanelResources(IGraphics* pGraphics)
{
  return PanelResources(pGraphics);
}

inline void AttachOutputMeterPanel(IGraphics* pGraphics, const PanelResources& resources, int outMeterTag)
{
  const IRECT outMeterBounds = IRECT::MakeXYWH(853.f, 11.f, 226.f, 46.f);
  auto* outMeter = new IVLEDMeterControl<2>(
    outMeterBounds, "", resources.meterStyle, EDirection::Horizontal, {}, 26, MakeOutputMeterLEDRanges());
  outMeter->SetResponse(IVMeterControl<2>::EResponse::Linear);
  pGraphics->AttachControl(outMeter, outMeterTag);
}

inline KeyboardControl* GetKeyboardControl(IGraphics* pGraphics, int keyboardTag)
{
  if(auto* control = pGraphics->GetControlWithTag(keyboardTag))
    return control->As<KeyboardControl>();
  return nullptr;
}

inline void SetKeyboardKeyNoteHighlight(IGraphics* pGraphics, int keyboardTag, int midiNote, bool highlighted)
{
  if(auto* keyboard = GetKeyboardControl(pGraphics, keyboardTag))
    keyboard->SetHighlightedMidiNote(midiNote, highlighted);
}

inline void RefreshKeyboardKeyNoteHighlights(KeyboardControl* keyboardControl, const CompoundPreset& compoundPreset)
{
  keyboardControl->ClearHighlightedMidiNotes();
  for(int midiNote = CompoundPreset::kMinMidiNote; midiNote <= CompoundPreset::kMaxMidiNote; ++midiNote)
    keyboardControl->SetHighlightedMidiNote(midiNote, compoundPreset.HasKeyNotePreset(midiNote));
}

struct VizEditPanelControls
{
  IVSlideSwitchControl* modeSwitch = nullptr;
  std::function<void(bool)> setMainPanelMode;
};

inline VizEditPanelControls AttachVizEditPanelControls(IGraphics* pGraphics,
                                                       const PanelResources& resources,
                                                       const std::shared_ptr<editor::EditorContext>& context,
                                                       int harmonicVisualizerTag,
                                                       int editorTabsTag,
                                                       int keyboardTag,
                                                       int breathMeterTag)
{
  const IRECT mainPanelModeSwitchBounds = IRECT::MakeXYWH(73.f, 442.f, 42.f, 26.f);
  const IRECT addButtonBounds = IRECT::MakeXYWH(14.f, 477.f, 75.f, 26.f);
  const IRECT deleteButtonBounds = IRECT::MakeXYWH(99.f, 477.f, 75.f, 26.f);

  const auto setMainPanelVizVisible = [pGraphics, breathMeterTag, harmonicVisualizerTag](bool visible) {
    if(auto* breathMeter = pGraphics->GetControlWithTag(breathMeterTag))
      breathMeter->Hide(!visible);
    if(auto* harmonicVisualizer = pGraphics->GetControlWithTag(harmonicVisualizerTag))
      harmonicVisualizer->Hide(!visible);
  };
  const auto setMainPanelEditVisible = [pGraphics, editorTabsTag](bool visible) {
    if(auto* editorTabs = pGraphics->GetControlWithTag(editorTabsTag))
      editorTabs->Hide(!visible);
  };
  const auto setKeyboardEditMode = [pGraphics, keyboardTag](bool editMode) {
    if(auto* keyboard = GetKeyboardControl(pGraphics, keyboardTag))
      keyboard->SetEditMode(editMode);
  };
  const auto setMainPanelMode = [context, setMainPanelVizVisible, setMainPanelEditVisible, setKeyboardEditMode](bool vizMode) {
    setMainPanelVizVisible(vizMode);
    setMainPanelEditVisible(!vizMode);
    setKeyboardEditMode(!vizMode);
    context->SetEditMode(!vizMode);
    context->RefreshEditorActionButtons();
  };

  auto* mainPanelModeSwitch = new IVSlideSwitchControl(
    mainPanelModeSwitchBounds,
    [setMainPanelMode](IControl* caller) {
      setMainPanelMode(caller->GetValue() < 0.5);
    },
    "",
    resources.mainPanelModeSwitchStyle,
    false,
    EDirection::Horizontal,
    2,
    context->IsEditMode() ? 1 : 0);
  pGraphics->AttachControl(mainPanelModeSwitch);

  auto* addButtonControl = new IVButtonControl(
    addButtonBounds, SplashClickActionFunc, "ADD", resources.vizEditButtonStyle, true, false);
  addButtonControl->SetAnimationEndActionFunction(
    [pGraphics, keyboardTag, context](IControl* caller) {
      if(!caller || !context->HasValidSelectedMidiNote())
        return;

      const int midiNote = context->SelectedMidiNote();
      if(context->Preset().HasKeyNotePreset(midiNote))
        return;

      context->Preset().SetKeyNotePreset(midiNote, context->Preset().GetPresetForMidiNote(midiNote));
      context->SendKeyNotePresetEditToDSP(caller, editor_messages::kMsgTagAddKeyNotePreset, midiNote);
      SetKeyboardKeyNoteHighlight(pGraphics, keyboardTag, midiNote, true);
      context->RefreshOscillatorTabs();
      context->RefreshEditorActionButtons();
    });

  auto* deleteButtonControl = new IVButtonControl(
    deleteButtonBounds, SplashClickActionFunc, "DELETE", resources.vizEditButtonStyle, true, false);
  deleteButtonControl->SetAnimationEndActionFunction(
    [pGraphics, keyboardTag, context](IControl* caller) {
      if(!caller || !context->HasValidSelectedMidiNote())
        return;

      const int midiNote = context->SelectedMidiNote();
      if(!context->Preset().RemoveKeyNotePreset(midiNote))
        return;

      context->SendKeyNotePresetEditToDSP(caller, editor_messages::kMsgTagRemoveKeyNotePreset, midiNote);
      SetKeyboardKeyNoteHighlight(pGraphics, keyboardTag, midiNote, false);
      context->RefreshOscillatorTabs();
      context->RefreshEditorActionButtons();
    });

  pGraphics->AttachControl(addButtonControl);
  pGraphics->AttachControl(deleteButtonControl);
  *context->buttons.addButton = addButtonControl;
  *context->buttons.deleteButton = deleteButtonControl;

  return {mainPanelModeSwitch, setMainPanelMode};
}

inline void AttachKeyboardPanelControls(IGraphics* pGraphics,
                                        const std::shared_ptr<editor::EditorContext>& context,
                                        int keyboardTag,
                                        int benderTag)
{
  const IRECT wheelsBounds = IRECT::MakeXYWH(6.f, 522.f, 35.f, 114.f);
  const IRECT keyboardBounds = IRECT::MakeXYWH(40.f, 522.f, 1038.f, 114.f);

  auto* keyboardControl = new KeyboardControl(keyboardBounds, 21, 108);
  RefreshKeyboardKeyNoteHighlights(keyboardControl, context->Preset());
  keyboardControl->SetOnSelectedMidiNoteChanged([context](int midiNote) {
    context->SetSelectedMidiNote(midiNote);
    context->RefreshOscillatorTabs();
    context->RefreshEditorActionButtons();
  });
  keyboardControl->SetSelectedMidiNote(context->SelectedMidiNote());

  pGraphics->AttachControl(new IWheelControl(wheelsBounds), benderTag);
  pGraphics->AttachControl(keyboardControl, keyboardTag);
}

inline void AttachEnvelopePanelControls(IGraphics* pGraphics,
                                        const PanelResources& resources)
{
  const std::array<LabelledKnobValueSpec, 2> envelopeKnobs{{
    {{IRECT::MakeMidXYWH(216.f, 480.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(245.f, 481.f, 70.f, 12.f), kParamGlobalAttackScale}, "Attack", IRECT::MakeXYWH(245.f, 468.f, 70.f, 12.f)},
    {{IRECT::MakeMidXYWH(312.f, 480.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(343.f, 481.f, 70.f, 12.f), kParamGlobalReleaseScale}, "Release", IRECT::MakeXYWH(343.f, 468.f, 70.f, 12.f)}
  }};

  for(const auto& spec : envelopeKnobs)
    AttachLabelledKnobWithValue(pGraphics, resources.knobAssets, spec, resources.compactLabelText, resources.compactValueText);
}

inline void AttachPitchPanelControls(IGraphics* pGraphics,
                                     const PanelResources& resources)
{
  const LabelledKnobValueSpec pitchKnob{
    {IRECT::MakeMidXYWH(431.f, 480.5f, resources.knobSize, resources.knobSize),
     IRECT::MakeXYWH(464.f, 481.f, 70.f, 12.f),
     kParamGlobalPitchShift},
    "Pitch",
    IRECT::MakeXYWH(464.f, 468.f, 70.f, 12.f)
  };

  AttachLabelledKnobWithValue(pGraphics, resources.knobAssets, pitchKnob, resources.compactLabelText, resources.compactValueText);
  AttachPassiveCaption(pGraphics, IRECT::MakeXYWH(527.5f, 470.f, 50.f, 20.f), kParamPortamentoAtCC5Min, resources.portamentoValueText);
  AttachPassiveCaption(pGraphics, IRECT::MakeXYWH(611.f, 489.f, 50.f, 20.f), kParamPortamentoAtCC5Max, resources.portamentoValueText);
  pGraphics->AttachControl(new IVRangeSliderControl(
    IRECT::MakeXYWH(524.f, 474.f, 140.f, 30.f),
    {kParamPortamentoAtCC5Min, kParamPortamentoAtCC5Max},
    "",
    resources.portamentoRangeSliderStyle,
    EDirection::Horizontal,
    true,
    9.f,
    3.f));
  AttachPassiveText(pGraphics, IRECT::MakeXYWH(532.5f, 455.f, 70.f, 12.f), "Portamento", resources.compactLabelText);
}

inline void AttachVariationPanelControls(IGraphics* pGraphics,
                                         const PanelResources& resources)
{
  const std::array<KnobValueSpec, 6> variationKnobs{{
    {IRECT::MakeMidXYWH(923.f, 141.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(951.f, 134.f, 70.f, 12.f), kParamGlobalIntensityVariationAmplitudeScale},
    {IRECT::MakeMidXYWH(1018.f, 141.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1046.f, 134.f, 70.f, 12.f), kParamGlobalIntensityVariationRateScale},
    {IRECT::MakeMidXYWH(923.f, 195.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(951.f, 188.f, 70.f, 12.f), kParamGlobalPanVariationAmplitudeScale},
    {IRECT::MakeMidXYWH(1018.f, 195.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1046.f, 188.f, 70.f, 12.f), kParamGlobalPanVariationRateScale},
    {IRECT::MakeMidXYWH(923.f, 249.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(951.f, 242.f, 70.f, 12.f), kParamGlobalPitchVariationAmplitudeScale},
    {IRECT::MakeMidXYWH(1018.f, 249.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1046.f, 242.f, 70.f, 12.f), kParamGlobalPitchVariationRateScale}
  }};

  for(const auto& spec : variationKnobs)
    AttachKnobWithValue(pGraphics, resources.knobAssets, spec, resources.compactValueText);
}

inline void AttachOutputPanelControls(IGraphics* pGraphics,
                                      const PanelResources& resources)
{
  const std::array<LabelledKnobValueSpec, 2> outputKnobs{{
    {{IRECT::MakeMidXYWH(886.f, 342.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(918.f, 343.f, 50.f, 12.f), kParamGlobalPanShift}, "Pan", IRECT::MakeXYWH(918.f, 330.f, 50.f, 12.f)},
    {{IRECT::MakeMidXYWH(992.f, 342.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1024.f, 343.f, 50.f, 12.f), kParamGlobalLevel}, "Level", IRECT::MakeXYWH(1024.f, 330.f, 50.f, 12.f)}
  }};

  for(const auto& spec : outputKnobs)
    AttachLabelledKnobWithValue(pGraphics, resources.knobAssets, spec, resources.compactLabelText, resources.compactValueText);
}
} // namespace layout

inline std::shared_ptr<editor::EditorContext> AttachMainControls(IGraphics* pGraphics,
                                                                 const std::shared_ptr<EditorState>& editorState,
                                                                 int harmonicVisualizerTag,
                                                                 int editorTabsTag,
                                                                 int keyboardTag,
                                                                 int benderTag,
                                                                 int breathMeterTag,
                                                                 int outMeterTag)
{
  const layout::PanelResources resources = layout::MakePanelResources(pGraphics);
  auto context = AttachEditorMainControls(pGraphics, editorState, harmonicVisualizerTag, editorTabsTag, breathMeterTag);

  // Title panel: x=4, y=4, w=840, h=60
  layout::AttachTitlePanelControls(pGraphics, context);

  // Output meter panel: x=846, y=4, w=240, h=60
  layout::AttachOutputMeterPanel(pGraphics, resources, outMeterTag);

  // Main panel: x=4, y=66, w=840, h=360
  // Viz/edit panel: x=4, y=428, w=180, h=84
  const auto vizEditPanel = layout::AttachVizEditPanelControls(
    pGraphics, resources, context, harmonicVisualizerTag, editorTabsTag, keyboardTag, breathMeterTag);

  // Keyboard panel: x=4, y=514, w=1082, h=130
  layout::AttachKeyboardPanelControls(pGraphics, context, keyboardTag, benderTag);
  context->RefreshEditorActionButtons();
  vizEditPanel.setMainPanelMode(!context->IsEditMode());
  vizEditPanel.modeSwitch->SetDirty(false);

  // Envelope panel: x=186, y=428, w=212, h=84
  layout::AttachEnvelopePanelControls(pGraphics, resources);

  // Pitch panel: x=400, y=428, w=262, h=84
  layout::AttachPitchPanelControls(pGraphics, resources);

  // Blip guard panel: x=664, y=428, w=180, h=84
  // TODO: add controls for blip guard settings here once implemented

  // Variation panel: x=846, y=66, w=240, h=222
  layout::AttachVariationPanelControls(pGraphics, resources);

  // Output panel: x=846, y=290, w=240, h=84
  layout::AttachOutputPanelControls(pGraphics, resources);
  
  // Effects panel: x=846, y=376, w=240, h=136
  // TODO: add controls for effects settings here once implemented

  return context;
}
} // namespace plugin_ui

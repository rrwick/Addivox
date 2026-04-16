#pragma once

#include "IControls.h"
#include "IVPresetManagerControls.h"
#include "about_built_with_control.h"
#include "colour.h"
#include "editor_messages.h"
#include "editor_panel.h"
#include "help_text.h"
#include "knob.h"
#include "number_box_control.h"
#include "theme.h"
#include "../settings/params.h"

#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace layout
{
inline bool ShowAboutBox(IGraphics* pGraphics, int aboutBoxTag)
{
  if(!pGraphics)
    return false;

  auto* control = pGraphics->GetControlWithTag(aboutBoxTag);
  if(!control)
    return false;

  auto* aboutBox = control->As<IAboutBoxControl>();
  if(!aboutBox)
    return false;

  aboutBox->Show();
  return true;
}

template <typename Callback>
inline IActionFunction MakeImmediateButtonAction(Callback&& callback)
{
  return [cb = std::forward<Callback>(callback)](IControl* caller) mutable {
    if(caller)
    {
      caller->SetValue(0.);
      caller->SetDirty(false);
    }

    cb(caller);
  };
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
    const auto restorePreset = [this](iplug::IPluginBase* pluginBase, int presetIdx) {
      if(!pluginBase || pluginBase->NPresets() == 0)
        return;

      pluginBase->RestorePreset(presetIdx);
      UpdatePresetLabel(pluginBase);
    };

    const auto prevPresetFunc = [restorePreset](IControl* caller) {
      auto* pluginBase = dynamic_cast<iplug::IPluginBase*>(caller->GetDelegate());
      if(!pluginBase || pluginBase->NPresets() == 0)
        return;

      int presetIdx = pluginBase->GetCurrentPresetIdx() - 1;
      if(presetIdx < 0)
        presetIdx = pluginBase->NPresets() - 1;

      restorePreset(pluginBase, presetIdx);
    };

    const auto nextPresetFunc = [restorePreset](IControl* caller) {
      auto* pluginBase = dynamic_cast<iplug::IPluginBase*>(caller->GetDelegate());
      if(!pluginBase || pluginBase->NPresets() == 0)
        return;

      int presetIdx = pluginBase->GetCurrentPresetIdx() + 1;
      if(presetIdx >= pluginBase->NPresets())
        presetIdx = 0;

      restorePreset(pluginBase, presetIdx);
    };

    const auto choosePresetFunc = [this](IControl* caller) {
      mPresetMenu.Clear();

      auto* pluginBase = dynamic_cast<iplug::IPluginBase*>(caller->GetDelegate());
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

    AddChildControl(new IVButtonControl(IRECT(), MakeImmediateButtonAction(prevPresetFunc), "<", mStyle));
    AddChildControl(new IVButtonControl(IRECT(), MakeImmediateButtonAction(nextPresetFunc), ">", mStyle));
    AddChildControl(mPresetNameButton = new IVButtonControl(IRECT(), MakeImmediateButtonAction(choosePresetFunc), "Choose Preset...", mStyle));

    OnResize();
    UpdatePresetLabel(dynamic_cast<iplug::IPluginBase*>(GetDelegate()));
  }

  void OnPopupMenuSelection(IPopupMenu* selectedMenu, int valIdx) override
  {
    if(!selectedMenu)
      return;

    auto* selectedItem = selectedMenu->GetChosenItem();
    if(!selectedItem)
      return;

    auto* pluginBase = dynamic_cast<iplug::IPluginBase*>(GetDelegate());
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
  void UpdatePresetLabel(iplug::IPluginBase* pluginBase)
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
      sections.ReduceFromLeft(30),
      sections.ReduceFromLeft(30),
      sections
    };

    return rects[static_cast<int>(control)];
  }

  IPopupMenu mPresetMenu;
  IVButtonControl* mPresetNameButton = nullptr;
};

class SettingsMenuButton final : public IVButtonControl
{
public:
  SettingsMenuButton(const IRECT& bounds,
                     const ISVG& gearIcon,
                     const std::shared_ptr<BreathCCSource>& breathCCSource,
                     const std::shared_ptr<bool>& harmonicVisualizerEnabled)
  : IVButtonControl(bounds, EmptyClickActionFunc, "", theme::AboutIconButtonStyle(), false, false, EVShape::Ellipse)
  , mGearIcon(gearIcon)
  , mBreathCCSource(breathCCSource)
  , mHarmonicVisualizerEnabled(harmonicVisualizerEnabled)
  {
    SetTooltip("Settings");
    SetActionFunction(MakeImmediateButtonAction([this](IControl*) { OpenMenu(); }));
  }

  void DrawWidget(IGraphics& g) override
  {
    const bool pressed = GetValue() > 0.5;
    DrawPressableShape(g, EVShape::Ellipse, mWidgetBounds, pressed, mMouseIsOver, IsDisabled());

    IColor iconColor = colour::ui::kValueText;
    if(IsDisabled())
      iconColor = GetColor(kFR);
    else if(pressed || mMouseIsOver)
      iconColor = GetColor(kX1);
    g.DrawSVG(mGearIcon, mWidgetBounds.GetPadded(-7.f), &mBlend, nullptr, &iconColor);
  }

  void OnPopupMenuSelection(IPopupMenu* selectedMenu, int valIdx) override
  {
    if(selectedMenu && selectedMenu->GetChosenItem())
    {
      const char* selectedText = selectedMenu->GetChosenItem()->GetText();
      if(selectedText && std::strcmp(selectedText, kVisualizerEnabledMenuLabel) == 0)
      {
        const bool enabled = !(mHarmonicVisualizerEnabled && *mHarmonicVisualizerEnabled);
        if(mHarmonicVisualizerEnabled)
          *mHarmonicVisualizerEnabled = enabled;

        if(auto* delegate = GetDelegate())
        {
          const editor_messages::SetHarmonicVisualizerEnabledPayload payload{enabled ? 1 : 0};
          delegate->SendArbitraryMsgFromUI(
            editor_messages::kMsgTagSetHarmonicVisualizerEnabled,
            GetTag(),
            sizeof(payload),
            &payload);
        }
      }
      else
      {
        BreathCCSource selectedSource = kDefaultBreathCCSource;
        if(TryParseBreathCCSourceMenuLabel(selectedText, selectedSource))
        {
          if(mBreathCCSource)
            *mBreathCCSource = selectedSource;

          if(auto* delegate = GetDelegate())
          {
            const editor_messages::SetBreathCCSourcePayload payload{static_cast<int>(selectedSource)};
            delegate->SendArbitraryMsgFromUI(
              editor_messages::kMsgTagSetBreathCCSource,
              GetTag(),
              sizeof(payload),
              &payload);
          }
        }
      }
    }

    IControl::OnPopupMenuSelection(selectedMenu, valIdx);
  }

private:
  void OpenMenu()
  {
    auto* ui = GetUI();
    if(!ui)
      return;

    BuildMenu();
    ui->CreatePopupMenu(*this, mMenu, mRECT);
  }

  void BuildMenu()
  {
    mMenu.Clear();
    const int visualizerFlags =
      (mHarmonicVisualizerEnabled && *mHarmonicVisualizerEnabled) ? IPopupMenu::Item::kChecked : 0;
    mMenu.AddItem(kVisualizerEnabledMenuLabel, -1, visualizerFlags);
    mMenu.AddSeparator();

    auto* breathMenu = new IPopupMenu("Breath CC");
    const BreathCCSource currentSource = mBreathCCSource ? *mBreathCCSource : kDefaultBreathCCSource;
    for(const BreathCCSource source : kBreathCCSources)
    {
      const int flags = (source == currentSource) ? IPopupMenu::Item::kChecked : 0;
      breathMenu->AddItem(GetBreathCCSourceMenuLabel(source), -1, flags);
    }

    mMenu.AddItem("Breath CC", breathMenu);
  }

  IPopupMenu mMenu{"Settings"};
  ISVG mGearIcon;
  std::shared_ptr<BreathCCSource> mBreathCCSource;
  std::shared_ptr<bool> mHarmonicVisualizerEnabled;

  static constexpr const char* kVisualizerEnabledMenuLabel = "Visualizer enabled";
};

inline void AttachTitlePanelControls(IGraphics* pGraphics,
                                     const std::shared_ptr<editor::EditorContext>& context,
                                     int aboutBoxTag)
{
  const auto sendPresetFileMessage = [](IControl* caller, int msgTag) {
    if(!caller)
      return;

    if(auto* delegate = caller->GetDelegate())
      delegate->SendArbitraryMsgFromUI(msgTag, caller->GetTag());
  };

  auto* presetManagerControl = new BakedPresetManagerControl(IRECT::MakeXYWH(325.f, 14.f, 270.f, 42.f), "", theme::PresetManagerStyle());

  auto* loadPresetButton = new IVButtonControl(
    IRECT::MakeXYWH(595.f, 14.f, 50.f, 42.f),
    MakeImmediateButtonAction([sendPresetFileMessage](IControl* caller) {
      sendPresetFileMessage(caller, editor_messages::kMsgTagPromptLoadPresetFromFile);
    }), "Load", theme::PresetActionButtonStyle(), true, false);

  auto* savePresetButton = new IVButtonControl(
    IRECT::MakeXYWH(645.f, 14.f, 50.f, 42.f),
    MakeImmediateButtonAction([sendPresetFileMessage](IControl* caller) {
      sendPresetFileMessage(caller, editor_messages::kMsgTagPromptSavePresetToFile);
    }), "Save", theme::PresetActionButtonStyle(), true, false);

  auto* settingsButton = new SettingsMenuButton(
    IRECT::MakeXYWH(707.f, 19.f, 32.f, 32.f),
    pGraphics->LoadSVG("gear.svg"),
    context->model.breathCCSource,
    context->model.harmonicVisualizerEnabled);

  auto* aboutButton = new IVButtonControl(
    IRECT::MakeXYWH(745.f, 19.f, 32.f, 32.f),
    MakeImmediateButtonAction([pGraphics, aboutBoxTag](IControl*) {
      ShowAboutBox(pGraphics, aboutBoxTag);
    }),
    "i",
    theme::AboutIconButtonStyle(),
    true,
    false);
  aboutButton->SetTooltip("About Addivox");

  pGraphics->AttachControl(aboutButton);
  pGraphics->AttachControl(settingsButton);
  pGraphics->AttachControl(loadPresetButton);
  pGraphics->AttachControl(savePresetButton);
  pGraphics->AttachControl(presetManagerControl);
  *context->title.presetManagerControl = presetManagerControl;
}

struct PanelResources
{
  explicit PanelResources(IGraphics* pGraphics)
  : knobAssets{pGraphics->LoadSVG("knob-fixed.svg"), pGraphics->LoadSVG("knob-rotating.svg")}
  , aboutLogo(pGraphics->LoadSVG("about-logo.svg"))
  {
  }

  float knobSize = 52.f;
  IVStyle meterStyle = theme::MeterStyle();
  IVStyle vizEditButtonStyle = theme::VizEditButtonStyle();
  IVStyle mainPanelModeSwitchStyle = theme::MainPanelModeSwitchStyle();
  IVStyle numberBoxStyle = theme::BaseStyle(false, false)
    .WithValueText(IText(14.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle));
  IVStyle portamentoRangeSliderStyle = theme::PortamentoRangeSliderStyle();
  IText compactLabelText = theme::CompactLabelText();
  IText compactValueText = theme::CompactValueText();
  IText portamentoValueText = theme::PortamentoValueText();
  KnobAssets knobAssets;
  ISVG aboutLogo;
};

inline PanelResources MakePanelResources(IGraphics* pGraphics)
{
  return PanelResources(pGraphics);
}

inline void AttachOutputMeterPanel(IGraphics* pGraphics, const PanelResources& resources, int outMeterTag)
{
  const IRECT outMeterBounds = IRECT::MakeXYWH(790.f, 11.f, 200.f, 46.f);
  auto* outMeter = new IVLEDMeterControl<2>(
    outMeterBounds, "", resources.meterStyle, EDirection::Horizontal, {}, 26, MakeOutputMeterLEDRanges());
  outMeter->SetResponse(IVMeterControl<2>::EResponse::Linear);
  pGraphics->AttachControl(outMeter, outMeterTag);
}

inline void AttachAboutBoxControl(IGraphics* pGraphics, const PanelResources& resources, int aboutBoxTag)
{
  const ISVG aboutLogo = resources.aboutLogo;
  pGraphics->AttachControl(
    new IAboutBoxControl(
      pGraphics->GetBounds(),
      IColor{242, 8, 10, 12},
      [aboutLogo](IContainerBase* pParent, const IRECT&) {
        pParent->AddChildControl(new ISVGControl(IRECT(), aboutLogo));
        pParent->AddChildControl(new ITextControl(IRECT(), "an additive synthesizer for wind controllers", theme::AboutMetaText(30.f), COLOR_TRANSPARENT));
        pParent->AddChildControl(new IURLControl(IRECT(), PLUG_URL_DISPLAY_STR, PLUG_URL_STR, theme::AboutLinkText(), COLOR_TRANSPARENT, COLOR_WHITE, colour::ui::kAccentPrimary));
        pParent->AddChildControl(new ITextControl(IRECT(), "version " PLUG_VERSION_STR, theme::AboutMetaText(), COLOR_TRANSPARENT));
        pParent->AddChildControl(new ITextControl(IRECT(), PLUG_COPYRIGHT_STR, theme::AboutMetaText(), COLOR_TRANSPARENT));
        pParent->AddChildControl(new AboutBuiltWithControl(IRECT(), "https://iplug2.github.io/"));
      },
      [](IContainerBase* pParent, const IRECT& r) {
        const IRECT content = r.GetCentredInside(920.f, 320.f);
        const IRECT urlRow = IRECT::MakeXYWH(content.L + 24.f, content.T + 210.f, content.W() - 48.f, 24.f);
        IRECT urlTextBounds;
        pParent->GetUI()->MeasureText(theme::AboutLinkText(), PLUG_URL_DISPLAY_STR, urlTextBounds);

        pParent->GetChild(0)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(content.MW() - 410.f, content.T +  30.f, 820.f, 112.f));
        pParent->GetChild(1)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(content.L,            content.T + 160.f, content.W(),        30.f));
        pParent->GetChild(2)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(urlRow.MW() - (urlTextBounds.W() * 0.5f), urlRow.T, urlTextBounds.W(), urlRow.H()));
        pParent->GetChild(3)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(content.L,            content.T + 240.f, content.W(),        24.f));
        pParent->GetChild(4)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(content.L + 24.f,     content.T + 270.f, content.W() - 48.f, 24.f));
        pParent->GetChild(5)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(content.L,            content.T + 300.f, content.W(),        24.f));
      },
      180),
    aboutBoxTag)->Hide(true);
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
  addButtonControl->SetTooltip(help_text::oscillator_tabs::kAddButton);
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
  deleteButtonControl->SetTooltip(help_text::oscillator_tabs::kDeleteButton);
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
  const IRECT wheelsBounds = IRECT::MakeXYWH(4.f, 630.f, 35.f, 110.f);
  const IRECT keyboardBounds = IRECT::MakeXYWH(38.f, 630.f, 952.f, 110.f);

  auto* keyboardControl = new KeyboardControl(keyboardBounds, 21, 108);
  RefreshKeyboardKeyNoteHighlights(keyboardControl, context->Preset());
  keyboardControl->SetOnSelectedMidiNoteChanged([context](int midiNote) {
    context->SetSelectedMidiNote(midiNote);
    context->RefreshOscillatorTabs();
    context->RefreshEditorActionButtons();
});
  keyboardControl->SetSelectedMidiNote(context->SelectedMidiNote());
  keyboardControl->SetTooltip(help_text::main_ui::kKeyboard);

  auto* wheelControl = new IWheelControl(wheelsBounds);
  wheelControl->SetTooltip(help_text::main_ui::kPitchBendWheel);
  pGraphics->AttachControl(wheelControl, benderTag);
  pGraphics->AttachControl(keyboardControl, keyboardTag);
}

inline void AttachEnvelopePanelControls(IGraphics* pGraphics,
                                        const PanelResources&)
{
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(190.f, 550.f, 50.f, 60.f), kParamGlobalAttackScale, "Attack", 3.f));
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(286.f, 550.f, 50.f, 60.f), kParamGlobalReleaseScale, "Release", 5.f));
}

inline void AttachPitchPanelControls(IGraphics* pGraphics,
                                     const PanelResources& resources)
{
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(484.f, 550.f, 50.f, 60.f), kParamGlobalPitchShift, "Pitch", 7.f));

  AttachPassiveText(pGraphics, IRECT::MakeXYWH(564.f, 594.f, 70.f, 12.f), "Portamento", resources.compactLabelText, help_text::main_ui::kPortamento);
  AttachPassiveCaption(pGraphics, IRECT::MakeXYWH(533.5f, 554.f, 50.f, 20.f), kParamPortamentoAtCC5Min, resources.portamentoValueText, help_text::main_ui::kPortamento);
  AttachPassiveCaption(pGraphics, IRECT::MakeXYWH(609.f, 573.f, 50.f, 20.f), kParamPortamentoAtCC5Max, resources.portamentoValueText, help_text::main_ui::kPortamento);
  auto* portamentoControl = new IVRangeSliderControl(IRECT::MakeXYWH(530.f, 558.f, 132.f, 30.f), {kParamPortamentoAtCC5Min, kParamPortamentoAtCC5Max}, "", resources.portamentoRangeSliderStyle, EDirection::Horizontal, true, 9.f, 3.f);
  portamentoControl->SetTooltip(help_text::main_ui::kPortamento);
  pGraphics->AttachControl(portamentoControl);

  AttachPassiveText(pGraphics, IRECT::MakeXYWH(420.f, 594.f, 80.f, 12.f), "Transpose", resources.compactLabelText, help_text::main_ui::kTranspose);
  auto* transposeControl = new NumberBoxControl(IRECT::MakeXYWH(420.f, 560.f, 58.f, 30.f), kParamTranspose, resources.numberBoxStyle, 0.0, -36.0, 36.0, "%0.0f");
  pGraphics->AttachControl(transposeControl);
  transposeControl->SetTooltip(help_text::main_ui::kTranspose);
}

inline void AttachVariationPanelControls(IGraphics* pGraphics,
                                         const PanelResources& resources)
{
  const std::array<KnobValueSpec, 6> variationKnobs{{
    {IRECT::MakeMidXYWH(983.f, 141.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1011.f, 134.f, 70.f, 12.f), kParamGlobalIntensityVariationAmplitudeScale},
    {IRECT::MakeMidXYWH(1078.f, 141.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1106.f, 134.f, 70.f, 12.f), kParamGlobalIntensityVariationRateScale},
    {IRECT::MakeMidXYWH(983.f, 195.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1011.f, 188.f, 70.f, 12.f), kParamGlobalPanVariationAmplitudeScale},
    {IRECT::MakeMidXYWH(1078.f, 195.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1106.f, 188.f, 70.f, 12.f), kParamGlobalPanVariationRateScale},
    {IRECT::MakeMidXYWH(983.f, 249.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1011.f, 242.f, 70.f, 12.f), kParamGlobalPitchVariationAmplitudeScale},
    {IRECT::MakeMidXYWH(1078.f, 249.5f, resources.knobSize, resources.knobSize), IRECT::MakeXYWH(1106.f, 242.f, 70.f, 12.f), kParamGlobalPitchVariationRateScale}
  }};

  for(const auto& spec : variationKnobs)
    AttachKnobWithValue(pGraphics, resources.knobAssets, spec, resources.compactValueText);
}

inline void AttachOutputPanelControls(IGraphics* pGraphics,
                                      const PanelResources&)
{
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(670.f, 550.f, 50.f, 60.f), kParamGlobalPanShift, "Pan"));
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(720.f, 550.f, 50.f, 60.f), kParamGlobalLevel, "Level"));
}

inline void AttachEffectsPanelControls(IGraphics* pGraphics,
                                       const PanelResources&)
{
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(790.f, 550.f, 50.f, 60.f), kParamEffectsDrive, "Drive"));
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(840.f, 550.f, 50.f, 60.f), kParamEffectsTone, "Tone"));
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(890.f, 550.f, 50.f, 60.f), kParamEffectsChorus, "Chorus"));
  pGraphics->AttachControl(new LabelledKnob(IRECT::MakeXYWH(940.f, 550.f, 50.f, 60.f), kParamEffectsReverb, "Reverb"));
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
  
  // Main panel: x=4, y=66, w=900, h=360
  auto context = AttachEditorMainControls(pGraphics, editorState, harmonicVisualizerTag, editorTabsTag, breathMeterTag);

  // Title panel: x=4, y=4, w=900, h=60
  layout::AttachTitlePanelControls(pGraphics, context, kCtrlTagAboutBox);

  // Output meter panel: x=906, y=4, w=240, h=60
  layout::AttachOutputMeterPanel(pGraphics, resources, outMeterTag);

  // Viz/edit panel: x=4, y=428, w=180, h=84
  const auto vizEditPanel = layout::AttachVizEditPanelControls(pGraphics, resources, context, harmonicVisualizerTag, editorTabsTag, keyboardTag, breathMeterTag);

  // Envelope panel: x=186, y=428, w=212, h=84
  layout::AttachEnvelopePanelControls(pGraphics, resources);

  // Pitch panel: x=400, y=428, w=322, h=84
  layout::AttachPitchPanelControls(pGraphics, resources);

  // Variation panel: x=906, y=66, w=240, h=222
  layout::AttachVariationPanelControls(pGraphics, resources);

  // Output panel: x=906, y=290, w=240, h=84
  layout::AttachOutputPanelControls(pGraphics, resources);
  
  // Effects panel: x=906, y=376, w=240, h=136
  layout::AttachEffectsPanelControls(pGraphics, resources);

  // Keyboard panel: x=4, y=514, w=1142, h=130
  layout::AttachKeyboardPanelControls(pGraphics, context, keyboardTag, benderTag);
  layout::AttachAboutBoxControl(pGraphics, resources, kCtrlTagAboutBox);
  context->RefreshEditorActionButtons();
  vizEditPanel.setMainPanelMode(!context->IsEditMode());
  vizEditPanel.modeSwitch->SetDirty(false);

  if(pGraphics->TooltipsEnabled())
    pGraphics->UpdateTooltips();

  return context;
}
} // namespace plugin_ui

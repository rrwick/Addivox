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
#include "positions.h"
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

inline void AttachPassiveText(IGraphics* pGraphics, const IRECT& bounds, const char* text, const IText& style, const char* tooltip = nullptr)
{
  auto* control = MakePassiveControl(new ITextControl(bounds, text, style, COLOR_TRANSPARENT));
  SetTooltipIfPresent(control, tooltip);
  pGraphics->AttachControl(control);
}

inline void AttachPassiveSectionLabel(IGraphics* pGraphics,
                                      const IRECT& bounds,
                                      const char* text,
                                      float angle = -90.f,
                                      const char* tooltip = nullptr)
{
  AttachPassiveText(pGraphics, bounds, text, theme::SectionLabelText(angle), tooltip);
}

inline void AttachPassiveCaption(IGraphics* pGraphics, const IRECT& bounds, int paramIdx,  const IText& style, const char* tooltip = nullptr)
{
  auto* control = MakePassiveControl(new ICaptionControl(bounds, paramIdx, style, COLOR_TRANSPARENT, true));
  SetTooltipIfPresent(control, tooltip);
  pGraphics->AttachControl(control);
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
      else if(selectedText && std::strcmp(selectedText, kResetToDefaultsMenuLabel) == 0)
      {
        if(auto* delegate = GetDelegate())
        {
          delegate->SendArbitraryMsgFromUI(
            editor_messages::kMsgTagResetStandaloneStateToDefaults,
            GetTag());
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
    mMenu.AddSeparator();
    mMenu.AddItem(kResetToDefaultsMenuLabel);
  }

  IPopupMenu mMenu{"Settings"};
  ISVG mGearIcon;
  std::shared_ptr<BreathCCSource> mBreathCCSource;
  std::shared_ptr<bool> mHarmonicVisualizerEnabled;

  static constexpr const char* kVisualizerEnabledMenuLabel = "Visualizer enabled";
  static constexpr const char* kResetToDefaultsMenuLabel = "Reset to defaults";
};

inline void AttachTitleControls(IGraphics* pGraphics, const std::shared_ptr<editor::EditorContext>& context, int aboutBoxTag)
{
  const auto sendPresetFileMessage = [](IControl* caller, int msgTag) {
    if(!caller)
      return;

    if(auto* delegate = caller->GetDelegate())
      delegate->SendArbitraryMsgFromUI(msgTag, caller->GetTag());
  };

  auto* presetManagerControl = new BakedPresetManagerControl(positions::kPresetManager, "", theme::PresetManagerStyle());

  auto* loadPresetButton = new IVButtonControl(
    positions::kLoadPresetButton,
    MakeImmediateButtonAction([sendPresetFileMessage](IControl* caller) {
      sendPresetFileMessage(caller, editor_messages::kMsgTagPromptLoadPresetFromFile);
    }), "Load", theme::PresetActionButtonStyle(), true, false);

  auto* savePresetButton = new IVButtonControl(
    positions::kSavePresetButton,
    MakeImmediateButtonAction([sendPresetFileMessage](IControl* caller) {
      sendPresetFileMessage(caller, editor_messages::kMsgTagPromptSavePresetToFile);
    }), "Save", theme::PresetActionButtonStyle(), true, false);

  auto* settingsButton = new SettingsMenuButton(
    positions::kSettingsButton,
    pGraphics->LoadSVG("gear.svg"), context->model.breathCCSource, context->model.harmonicVisualizerEnabled);

  auto* aboutButton = new IVButtonControl(
    positions::kAboutButton,
    MakeImmediateButtonAction([pGraphics, aboutBoxTag](IControl*) {
      ShowAboutBox(pGraphics, aboutBoxTag);
    }),
    "i", theme::AboutIconButtonStyle(), true, false);
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
    .WithValueText(IText(16.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle));
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

inline void AttachOutputMeterControls(IGraphics* pGraphics,
                                      const PanelResources& resources,
                                      int breathMeterTag,
                                      int outMeterTag)
{
  AttachPassiveText(pGraphics, positions::kBreathLabel, "Breath", resources.compactLabelText);
  AttachPassiveText(pGraphics, positions::kMainOutLabel, "Main Out", resources.compactLabelText);

  const IVStyle breathMeterStyle = resources.meterStyle.WithColor(kHL, COLOR_TRANSPARENT);
  pGraphics->AttachControl(new IVMeterControl<1>(positions::kBreathMeter, "", breathMeterStyle, EDirection::Horizontal), breathMeterTag);
  auto* outMeter = new IVLEDMeterControl<2>(positions::kOutputMeter, "", resources.meterStyle, EDirection::Horizontal, {}, 26, MakeOutputMeterLEDRanges());
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
        const IRECT content = r.GetCentredInside(positions::kAboutContent.W(), positions::kAboutContent.H());
        IRECT urlTextBounds;
        pParent->GetUI()->MeasureText(theme::AboutLinkText(), PLUG_URL_DISPLAY_STR, urlTextBounds);

        pParent->GetChild(0)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(
          content.L + positions::kAboutLogo.L,
          content.T + positions::kAboutLogo.T,
          positions::kAboutLogo.W(),
          positions::kAboutLogo.H()));
        pParent->GetChild(1)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(
          content.L + positions::kAboutSubtitle.L,
          content.T + positions::kAboutSubtitle.T,
          positions::kAboutSubtitle.W(),
          positions::kAboutSubtitle.H()));
        pParent->GetChild(2)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(
          content.L + positions::kAboutUrlRow.MW() - (urlTextBounds.W() * 0.5f),
          content.T + positions::kAboutUrlRow.T,
          urlTextBounds.W(),
          positions::kAboutUrlRow.H()));
        pParent->GetChild(3)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(
          content.L + positions::kAboutVersion.L,
          content.T + positions::kAboutVersion.T,
          positions::kAboutVersion.W(),
          positions::kAboutVersion.H()));
        pParent->GetChild(4)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(
          content.L + positions::kAboutCopyright.L,
          content.T + positions::kAboutCopyright.T,
          positions::kAboutCopyright.W(),
          positions::kAboutCopyright.H()));
        pParent->GetChild(5)->SetTargetAndDrawRECTs(IRECT::MakeXYWH(
          content.L + positions::kAboutBuiltWith.L,
          content.T + positions::kAboutBuiltWith.T,
          positions::kAboutBuiltWith.W(),
          positions::kAboutBuiltWith.H()));
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

struct VizEditControls
{
  IVSlideSwitchControl* modeSwitch = nullptr;
  std::function<void(bool)> setMainPanelMode;
};

inline VizEditControls AttachVizEditControls(IGraphics* pGraphics,
                                             const PanelResources& resources,
                                             const std::shared_ptr<editor::EditorContext>& context,
                                             int harmonicVisualizerTag,
                                             int editorTabsTag,
                                             int keyboardTag)
{
  const auto setMainPanelVizVisible = [pGraphics, harmonicVisualizerTag](bool visible) {
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
    positions::kModeSwitch,
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
  AttachPassiveSectionLabel(pGraphics, positions::kVizModeLabel, "VIS", 0.0);
  AttachPassiveSectionLabel(pGraphics, positions::kEditModeLabel, "EDIT", 0.0);

  return {mainPanelModeSwitch, setMainPanelMode};
}

inline void AttachKeyboardControls(IGraphics* pGraphics,
                                   const std::shared_ptr<editor::EditorContext>& context,
                                   int keyboardTag,
                                   int benderTag,
                                   int initialPitchBendRange)
{
  auto* keyboardControl = new KeyboardControl(positions::kKeyboard, 21, 108);
  RefreshKeyboardKeyNoteHighlights(keyboardControl, context->Preset());
  keyboardControl->SetOnSelectedMidiNoteChanged([context](int midiNote) {
    context->SetSelectedMidiNote(midiNote);
    context->RefreshOscillatorTabs();
    context->RefreshEditorActionButtons();
});
  keyboardControl->SetSelectedMidiNote(context->SelectedMidiNote());
  keyboardControl->SetTooltip(help_text::main_ui::kKeyboard);
  *context->keyboardControl = keyboardControl;

  auto* wheelControl = new IWheelControl(
    positions::kPitchBendWheel,
    IMidiMsg::EControlChangeMsg::kNoCC,
    initialPitchBendRange);
  wheelControl->SetTooltip(help_text::main_ui::kPitchBendWheel);
  pGraphics->AttachControl(wheelControl, benderTag);
  pGraphics->AttachControl(keyboardControl, keyboardTag);
}

inline void AttachEnvelopeControls(IGraphics* pGraphics, const PanelResources&)
{
  AttachPassiveSectionLabel(pGraphics, positions::kEnvelopeLabel, "ENVELOPE");
  pGraphics->AttachControl(new LabelledKnob(positions::kAttackKnob, kParamGlobalAttackScale, "Attack", 3.f));
  pGraphics->AttachControl(new LabelledKnob(positions::kReleaseKnob, kParamGlobalReleaseScale, "Release", 5.f));
}

inline void AttachPitchControls(IGraphics* pGraphics, const PanelResources& resources)
{
  AttachPassiveSectionLabel(pGraphics, positions::kPitchLabel, "PITCH");
  AttachPassiveText(pGraphics, positions::kTransposeLabel, "Transpose", resources.compactLabelText, help_text::main_ui::kTranspose);
  auto* transposeControl = new NumberBoxControl(positions::kTransposeNumberBox, kParamTranspose, resources.numberBoxStyle, 0.0, -36.0, 36.0, "%+0.0f");
  pGraphics->AttachControl(transposeControl);
  transposeControl->SetTooltip(help_text::main_ui::kTranspose);

  pGraphics->AttachControl(new LabelledKnob(positions::kTuningKnob, kParamGlobalTuning, "Tuning", 7.f));

  AttachPassiveText(pGraphics, positions::kPortamentoLabel, "Portamento", resources.compactLabelText, help_text::main_ui::kPortamento);
  AttachPassiveCaption(pGraphics, positions::kPortamentoMinCaption, kParamPortamentoAtCC5Min, resources.portamentoValueText, help_text::main_ui::kPortamento);
  AttachPassiveCaption(pGraphics, positions::kPortamentoMaxCaption, kParamPortamentoAtCC5Max, resources.portamentoValueText, help_text::main_ui::kPortamento);
  auto* portamentoControl = new IVRangeSliderControl(positions::kPortamentoRangeSlider, {kParamPortamentoAtCC5Min, kParamPortamentoAtCC5Max}, "", resources.portamentoRangeSliderStyle, EDirection::Horizontal, true, 9.f, 3.f);
  portamentoControl->SetTooltip(help_text::main_ui::kPortamento);
  pGraphics->AttachControl(portamentoControl);

}

inline void AttachVariationControls(IGraphics* pGraphics, const PanelResources&)
{
  AttachPassiveSectionLabel(pGraphics, positions::kVariationLabel, "VARIATION");
  pGraphics->AttachControl(new LabelledKnob(positions::kLevelAmountKnob, kParamGlobalIntensityVariationAmplitudeScale, "LvlAmt"));
  pGraphics->AttachControl(new LabelledKnob(positions::kLevelRateKnob, kParamGlobalIntensityVariationRateScale, "LvlRate"));
  pGraphics->AttachControl(new LabelledKnob(positions::kPanAmountKnob, kParamGlobalPanVariationAmplitudeScale, "PanAmt"));
  pGraphics->AttachControl(new LabelledKnob(positions::kPanRateKnob, kParamGlobalPanVariationRateScale, "PanRate"));
  pGraphics->AttachControl(new LabelledKnob(positions::kPitchAmountKnob, kParamGlobalPitchVariationAmplitudeScale, "PchAmt"));
  pGraphics->AttachControl(new LabelledKnob(positions::kPitchRateKnob, kParamGlobalPitchVariationRateScale, "PchRate"));
}

inline void AttachOutputControls(IGraphics* pGraphics, const PanelResources&)
{
  AttachPassiveSectionLabel(pGraphics, positions::kOutputLabel, "OUTPUT");
  pGraphics->AttachControl(new LabelledKnob(positions::kPanKnob, kParamGlobalPanShift, "Pan"));
  pGraphics->AttachControl(new LabelledKnob(positions::kLevelKnob, kParamGlobalLevel, "Level"));
}

inline void AttachEffectsControls(IGraphics* pGraphics, const PanelResources&)
{
  AttachPassiveSectionLabel(pGraphics, positions::kEffectsLabel, "EFFECTS");
  pGraphics->AttachControl(new LabelledKnob(positions::kDriveKnob, kParamEffectsDrive, "Drive"));
  pGraphics->AttachControl(new LabelledKnob(positions::kToneKnob, kParamEffectsTone, "Tone"));
  pGraphics->AttachControl(new LabelledKnob(positions::kChorusKnob, kParamEffectsChorus, "Chorus"));
  pGraphics->AttachControl(new LabelledKnob(positions::kReverbKnob, kParamEffectsReverb, "Reverb"));
}
} // namespace layout

inline std::shared_ptr<editor::EditorContext> AttachMainControls(IGraphics* pGraphics,
                                                                 const std::shared_ptr<EditorState>& editorState,
                                                                 int harmonicVisualizerTag,
                                                                 int editorTabsTag,
                                                                 int keyboardTag,
                                                                 int benderTag,
                                                                 int breathMeterTag,
                                                                 int outMeterTag,
                                                                 int initialPitchBendRange)
{
  const layout::PanelResources resources = layout::MakePanelResources(pGraphics);
  
  auto context = AttachEditorMainControls(pGraphics, editorState, harmonicVisualizerTag, editorTabsTag);
  layout::AttachTitleControls(pGraphics, context, kCtrlTagAboutBox);
  layout::AttachOutputMeterControls(pGraphics, resources, breathMeterTag, outMeterTag);
  const auto vizEditPanel = layout::AttachVizEditControls(pGraphics, resources, context, harmonicVisualizerTag, editorTabsTag, keyboardTag);
  layout::AttachEnvelopeControls(pGraphics, resources);
  layout::AttachPitchControls(pGraphics, resources);
  layout::AttachVariationControls(pGraphics, resources);
  layout::AttachOutputControls(pGraphics, resources);
  layout::AttachEffectsControls(pGraphics, resources);
  layout::AttachKeyboardControls(pGraphics, context, keyboardTag, benderTag, initialPitchBendRange);
  layout::AttachAboutBoxControl(pGraphics, resources, kCtrlTagAboutBox);
  context->RefreshEditorActionButtons();
  vizEditPanel.setMainPanelMode(!context->IsEditMode());
  vizEditPanel.modeSwitch->SetDirty(false);

  if(pGraphics->TooltipsEnabled())
    pGraphics->UpdateTooltips();

  return context;
}
} // namespace plugin_ui

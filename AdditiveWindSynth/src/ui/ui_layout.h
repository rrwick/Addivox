#pragma once

#include "IControls.h"
#include "colour.h"
#include "layered_svg_knob_control.h"
#include "oscillator_slider_control.h"
#include "preset_editor_keyboard_control.h"
#include "../visualizer/harmonic_visualizer_control.h"
#include "../settings/preset_key_notes.h"
#include "../settings/presets.h"
#include "../settings/settings_oscillator.h"
#include "../preset_editor_messages.h"

#include <algorithm>
#include <array>
#include <functional>
#include <memory>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace theme
{
inline IVStyle BaseStyle(bool showLabel = true, bool showValue = true)
{
  return DEFAULT_STYLE
    .WithShowLabel(showLabel)
    .WithShowValue(showValue)
    .WithLabelText(IText(13.f, colour::ui::kLabelText, "Roboto-Regular", EAlign::Center, EVAlign::Top))
    .WithValueText(IText(12.f, colour::ui::kValueText, "Roboto-Regular", EAlign::Center, EVAlign::Bottom))
    .WithDrawShadows(false)
    .WithDrawFrame(true)
    .WithFrameThickness(1.15f)
    .WithRoundness(0.2f)
    .WithColor(kBG, COLOR_TRANSPARENT)
    .WithColor(kFG, colour::ui::kControlBody)
    .WithColor(kPR, colour::ui::kAccentPrimary)
    .WithColor(kFR, colour::ui::kControlFrame)
    .WithColor(kHL, colour::ui::kControlHighlight)
    .WithColor(kSH, colour::ui::kControlShadow)
    .WithColor(kX1, colour::ui::kAccentPrimary)
    .WithColor(kX2, colour::ui::kAccentSecondary)
    .WithColor(kX3, colour::ui::kAccentPrimary);
}

inline IVStyle MeterStyle()
{
  return BaseStyle(true, false)
    .WithRoundness(0.1f)
    .WithColor(kBG, IColor{128, 0, 0, 0})
    .WithColor(kFG, colour::ui::kMeterForeground)
    .WithColor(kX1, colour::ui::kAccentSecondary)
    .WithColor(kHL, colour::ui::kMeterHighlight);
}
} // namespace theme

class EditorOscillatorTabPage final : public IVTabPage
{
public:
  using VisibilityChangedFunc = std::function<void(bool isVisible)>;

  EditorOscillatorTabPage(TabAttachFunc attachFunc, ResizeFunc resizeFunc, VisibilityChangedFunc visibilityChangedFunc)
  : IVTabPage(attachFunc, resizeFunc)
  , mVisibilityChangedFunc(visibilityChangedFunc)
  {
  }

  void Hide(bool hide) override
  {
    const bool wasHidden = IsHidden();
    IVTabPage::Hide(hide);
    const bool isHidden = IsHidden();
    if(wasHidden == isHidden)
      return;

    if(mVisibilityChangedFunc)
      mVisibilityChangedFunc(!isHidden);
  }

private:
  VisibilityChangedFunc mVisibilityChangedFunc{};
};

inline void AttachMainControls(
  IGraphics* pGraphics,
  int gainParamIdx,
  const std::array<int, 10>& globalModifierParamIdxs,
  int portamentoAtCC5MinParamIdx,
  int portamentoAtCC5MaxParamIdx,
  int harmonicVisualizerTag,
  int presetEditorTabsTag,
  int keyboardTag,
  int benderTag,
  int breathMeterTag,
  int outMeterTag)
{

  float knob_size = 52.f;
  const IVStyle knobStyle = theme::BaseStyle();
  const IText compactLabelText = IText(14.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Near);
  const IText compactValueText = IText(14.f, colour::ui::kValueText, "Roboto-Black", EAlign::Near);
  const IText compactValueTextRightAlign = IText(14.f, colour::ui::kValueText, "Roboto-Black", EAlign::Far);
  const IColor kSecondaryUIValueColor{255, 188, 188, 188};
  const IText portamentoValueText = IText(12.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center);
  const IVStyle meterStyle = theme::MeterStyle();
  const ISVG knobFixedSVG = pGraphics->LoadSVG("knob-fixed.svg");
  const ISVG knobRotatingSVG = pGraphics->LoadSVG("knob-rotating.svg");
  const auto attachKnob = [&](const IRECT& bounds, int paramIdx) {
    pGraphics->AttachControl(new LayeredSVGKnobControl(bounds, knobFixedSVG, knobRotatingSVG, paramIdx));
  };

  // Full UI: 1090 x 648

  // Title panel: x=4, y=4, w=840, h=60
  // TODO: add preset management controls here once implemented.

  // Output meter panel: x=846, y=4, w=240, h=60
  using OutMeterLEDRange = IVLEDMeterControl<2>::LEDRange;
  constexpr int kOutMeterNumBars = 26;  // Count and range are chosen so the first red bar is centered on 0 dBFS and therefore begins lighting just above 0 dB. So seeing the meter hit red indicates potential clipping.
  constexpr float kOutMeterLowDB = -73.5f;
  constexpr float kOutMeterHighDB = 4.5f;
  constexpr float kOutMeterStepDB = (kOutMeterHighDB - kOutMeterLowDB) / static_cast<float>(kOutMeterNumBars);
  const std::array<IColor, kOutMeterNumBars> outMeterLEDColors = {
    colour::visualizer::LED26, colour::visualizer::LED25,  // top levels (>0 dB) are bright red
    colour::visualizer::LED24, colour::visualizer::LED23, colour::visualizer::LED22, colour::visualizer::LED21,
    colour::visualizer::LED20, colour::visualizer::LED19, colour::visualizer::LED18, colour::visualizer::LED17,
    colour::visualizer::LED16, colour::visualizer::LED15, colour::visualizer::LED14, colour::visualizer::LED13,
    colour::visualizer::LED12, colour::visualizer::LED11, colour::visualizer::LED10, colour::visualizer::LED09,
    colour::visualizer::LED08, colour::visualizer::LED07, colour::visualizer::LED06, colour::visualizer::LED05,
    colour::visualizer::LED04, colour::visualizer::LED03, colour::visualizer::LED02, colour::visualizer::LED01
  };
  std::vector<OutMeterLEDRange> outMeterLEDRanges;
  outMeterLEDRanges.reserve(kOutMeterNumBars);
  for(int i = 0; i < kOutMeterNumBars; ++i)
  {
    const float lowDB = kOutMeterLowDB + (kOutMeterStepDB * static_cast<float>(i));
    const float highDB = lowDB + kOutMeterStepDB;
    outMeterLEDRanges.emplace_back(lowDB, highDB, 1, outMeterLEDColors[static_cast<std::size_t>(i)]);
  }
  const IRECT outMeterBounds = IRECT::MakeXYWH(853.f, 11.f, 226.f, 46.f);
  auto* meter = new IVLEDMeterControl<2>(outMeterBounds, "", meterStyle, EDirection::Horizontal, {}, kOutMeterNumBars, outMeterLEDRanges);
  meter->SetResponse(IVMeterControl<2>::EResponse::Linear); // disables marker drawing
  pGraphics->AttachControl(meter, outMeterTag);

  // Main panel: x=4, y=66, w=840, h=360
  // Viz mode: breath meter on the left and the visualizer taking up the rest of the space.
  // Edit mode: tabbed preset editor.
  const IRECT breathMeterBounds = IRECT::MakeXYWH(12.f, 113.f, 20.f, 266.f);
  const IRECT harmonicVisualizerBounds = IRECT::MakeXYWH(38.f, 74.f, 798.f, 344.f);
  const IRECT presetEditorTabsBounds = IRECT::MakeXYWH(12.f, 74.f, 824.f, 344.f);
  const IColor kPresetEditorDarkSurface = colour::kBackground;
  const IColor kPresetEditorDarkTab{255, 24, 32, 42};
  const IColor kPresetEditorBarColor{255, 118, 168, 230};
  const IVStyle presetEditorTabsStyle = theme::BaseStyle(false, false)
    .WithValueText(IText(13.f, colour::ui::kValueText, "Roboto-Bold", EAlign::Center, EVAlign::Middle))
    .WithColor(kFG, kPresetEditorDarkTab)
    .WithColor(kPR, kPresetEditorDarkSurface)
    .WithColor(kHL, IColor{70, 180, 210, 255});
  const IVStyle levelSliderStyle = theme::BaseStyle(false, false)
    .WithColor(kBG, kPresetEditorDarkSurface)
    .WithColor(kFG, kPresetEditorBarColor)
    .WithColor(kX1, kPresetEditorBarColor)
    .WithColor(kHL, IColor{55, 255, 255, 255});

  struct OscillatorTabDescriptor
  {
    const char* title;
    OscillatorSettings::Parameter parameter;
    OscillatorSliderControl::ValueRange range;
    const char* description;
  };

  const std::array<OscillatorTabDescriptor, OscillatorSettings::kNumParameters> oscillatorTabDescriptors{{
    {"Level", OscillatorSettings::Parameter::intensity, {0.0, 1.0},
      "Controls the intensity of each harmonic at full breath."},
    {"Breath", OscillatorSettings::Parameter::breath_power, {0.0, 100.0},
      "Controls how strongly breath scales each harmonic level."},
    {"Attack", OscillatorSettings::Parameter::attack, {0.0, 0.01},
      "Controls how quickly each harmonic ramps up at note start."},
    {"Release", OscillatorSettings::Parameter::release, {0.0, 0.1},
      "Controls how quickly each harmonic fades after note release."},
    {"Pitch", OscillatorSettings::Parameter::pitch, {-100.0, 100.0},
      "Controls static pitch offset per harmonic in cents."},
    {"Pan", OscillatorSettings::Parameter::pan, {-1.0, 1.0},
      "Controls stereo position of each harmonic from left to right."},
    {"LvlVarAmt", OscillatorSettings::Parameter::intensity_variation_amplitude, {0.0, 10.0},
      "Controls depth of intensity variation for each harmonic."},
    {"LvlVarRate", OscillatorSettings::Parameter::intensity_variation_rate, {0.0, 10.0},
      "Controls speed of intensity variation for each harmonic."},
    {"PchVarAmt", OscillatorSettings::Parameter::pitch_variation_amplitude, {0.0, 10.0},
      "Controls depth of pitch variation for each harmonic."},
    {"PchVarRate", OscillatorSettings::Parameter::pitch_variation_rate, {0.0, 10.0},
      "Controls speed of pitch variation for each harmonic."},
    {"PanVarAmt", OscillatorSettings::Parameter::pan_variation_amplitude, {0.0, 10.0},
      "Controls depth of pan variation for each harmonic."},
    {"PanVarRate", OscillatorSettings::Parameter::pan_variation_rate, {0.0, 10.0},
      "Controls speed of pan variation for each harmonic."},
  }};

  const IText oscillatorTabDescriptionText{13.f, colour::ui::kLabelText, "Roboto-Regular", EAlign::Near, EVAlign::Top};
  const IVStyle oscillatorRestoreButtonStyle = theme::BaseStyle(true, false)
    .WithLabelText(IText(13.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle))
    .WithColor(kFG, kPresetEditorDarkTab)
    .WithColor(kHL, IColor{55, 255, 255, 255});
  const auto oscillatorTabResizeFunc = [](IContainerBase* pTab, const IRECT& r) {
    if(pTab->NChildren() < 3)
      return;

    constexpr float kLeftInset = 104.f;
    constexpr float kDescriptionHeight = 112.f;
    constexpr float kButtonHeight = 24.f;
    constexpr float kGap = 8.f;
    auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
    auto leftColumnBounds = innerBounds.GetFromLeft(kLeftInset);
    const float descriptionBottom = std::min(leftColumnBounds.T + kDescriptionHeight, leftColumnBounds.B - (kButtonHeight + kGap + 2.f));
    auto descriptionBounds = IRECT(leftColumnBounds.L + 4.f, leftColumnBounds.T + 2.f, leftColumnBounds.R - 4.f, descriptionBottom);
    auto restoreButtonBounds = IRECT(leftColumnBounds.L + 8.f, descriptionBottom + kGap, leftColumnBounds.R - 8.f, descriptionBottom + kGap + kButtonHeight);
    auto sliderBounds = innerBounds;
    sliderBounds.L += kLeftInset;

    pTab->GetChild(0)->SetTargetAndDrawRECTs(descriptionBounds);
    pTab->GetChild(1)->SetTargetAndDrawRECTs(restoreButtonBounds);
    pTab->GetChild(2)->SetTargetAndDrawRECTs(sliderBounds);
  };

  auto editorCompoundPreset = std::make_shared<CompoundPreset>(presets::MakeBrassCompoundPreset());
  auto selectedEditorMidiNote = std::make_shared<int>(presets::kBrassCompoundPresetKeyNotes[3]);
  auto oscillatorSliderControls = std::make_shared<std::array<OscillatorSliderControl*, OscillatorSettings::kNumParameters>>();
  oscillatorSliderControls->fill(nullptr);
  auto restoreButtons = std::make_shared<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>>();
  restoreButtons->fill(nullptr);
  auto addButton = std::make_shared<IVButtonControl*>(nullptr);
  auto deleteButton = std::make_shared<IVButtonControl*>(nullptr);
  auto isEditorEditMode = std::make_shared<bool>(false);
  const auto sendOscillatorParameterToDSP = [presetEditorTabsTag](IControl* sourceControl,
                                                                   int midiNote,
                                                                   int oscillatorIndex,
                                                                   OscillatorSettings::Parameter parameter,
                                                                   double value) {
    if(!sourceControl)
      return;

    if(auto* delegate = sourceControl->GetDelegate())
    {
      preset_editor_messages::SetKeyNoteOscillatorParameterPayload payload;
      payload.midiNote = midiNote;
      payload.oscillatorIndex = oscillatorIndex;
      payload.parameter = static_cast<int>(parameter);
      payload.value = value;
      delegate->SendArbitraryMsgFromUI(
        preset_editor_messages::kMsgTagSetKeyNoteOscillatorParameter,
        presetEditorTabsTag,
        sizeof(payload),
        &payload);
    }
  };
  const auto sendKeyNotePresetEditToDSP = [presetEditorTabsTag](IControl* sourceControl, int msgTag, int midiNote) {
    if(!sourceControl)
      return;

    if(auto* delegate = sourceControl->GetDelegate())
    {
      preset_editor_messages::KeyNotePresetPayload payload;
      payload.midiNote = midiNote;
      delegate->SendArbitraryMsgFromUI(
        msgTag,
        presetEditorTabsTag,
        sizeof(payload),
        &payload);
    }
  };
  auto refreshEditorActionButtons = std::make_shared<std::function<void()>>();
  *refreshEditorActionButtons = [editorCompoundPreset, selectedEditorMidiNote, addButton, deleteButton, isEditorEditMode]() {
    const int selectedMidiNote = *selectedEditorMidiNote;
    const bool midiNoteValid = selectedMidiNote >= CompoundPreset::kMinMidiNote && selectedMidiNote <= CompoundPreset::kMaxMidiNote;
    const bool keyNoteSelected = midiNoteValid && editorCompoundPreset->HasKeyNotePreset(selectedMidiNote);
    const bool canAdd = *isEditorEditMode && midiNoteValid && !keyNoteSelected;
    const bool canDelete = *isEditorEditMode && keyNoteSelected;

    if(*addButton)
    {
      (*addButton)->SetDisabled(!canAdd);
      (*addButton)->SetDirty(false);
    }

    if(*deleteButton)
    {
      (*deleteButton)->SetDisabled(!canDelete);
      (*deleteButton)->SetDirty(false);
    }
  };
  auto refreshOscillatorTabs = std::make_shared<std::function<void()>>();
  *refreshOscillatorTabs = [editorCompoundPreset, selectedEditorMidiNote, oscillatorSliderControls, restoreButtons, oscillatorTabDescriptors]() {
    const int selectedMidiNote = *selectedEditorMidiNote;
    const bool midiNoteValid = selectedMidiNote >= CompoundPreset::kMinMidiNote && selectedMidiNote <= CompoundPreset::kMaxMidiNote;

    if(!midiNoteValid)
    {
      for(std::size_t i = 0; i < oscillatorSliderControls->size(); ++i)
      {
        auto* control = (*oscillatorSliderControls)[i];
        if(!control)
        {
          auto* restoreButton = (*restoreButtons)[i];
          if(restoreButton)
          {
            restoreButton->SetDisabled(true);
            restoreButton->SetDirty(false);
          }
          continue;
        }

        control->SetEditable(false);
        control->SetDirty(false);

        auto* restoreButton = (*restoreButtons)[i];
        if(restoreButton)
        {
          restoreButton->SetDisabled(true);
          restoreButton->SetDirty(false);
        }
      }
      return;
    }

    const SimplePreset* keyNotePreset = editorCompoundPreset->GetKeyNotePreset(selectedMidiNote);
    const SimplePreset& selectedPreset = keyNotePreset ? *keyNotePreset : editorCompoundPreset->GetPresetForMidiNote(selectedMidiNote);
    const bool editable = keyNotePreset != nullptr;

    for(const auto& descriptor : oscillatorTabDescriptors)
    {
      auto* control = (*oscillatorSliderControls)[static_cast<std::size_t>(descriptor.parameter)];
      if(!control)
        continue;

      for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
      {
        const double value = selectedPreset.GetOscillatorSettings(oscillatorIndex).GetParameter(descriptor.parameter);
        control->SetOscillatorValue(oscillatorIndex, value);
      }

      if(!control->IsHidden() && !control->HasRestoreStateForMidiNote(selectedMidiNote))
        control->CaptureRestoreState(selectedMidiNote);

      control->SetEditable(editable);
      control->SetDirty(false);

      auto* restoreButton = (*restoreButtons)[static_cast<std::size_t>(descriptor.parameter)];
      if(restoreButton)
      {
        restoreButton->SetDisabled(!(editable && control->HasRestoreStateForMidiNote(selectedMidiNote)));
        restoreButton->SetDirty(false);
      }
    }
  };

  const auto makeOscillatorTabPage =
    [oscillatorSliderControls,
      restoreButtons,
      refreshOscillatorTabs,
      editorCompoundPreset,
      selectedEditorMidiNote,
      levelSliderStyle,
      oscillatorTabDescriptionText,
      oscillatorRestoreButtonStyle,
      oscillatorTabResizeFunc,
      sendOscillatorParameterToDSP]
    (const OscillatorTabDescriptor& descriptor) {
      return new EditorOscillatorTabPage(
        [oscillatorSliderControls,
          restoreButtons,
          refreshOscillatorTabs,
          editorCompoundPreset,
          selectedEditorMidiNote,
          levelSliderStyle,
          oscillatorTabDescriptionText,
          oscillatorRestoreButtonStyle,
          sendOscillatorParameterToDSP,
          descriptor]
        (IVTabPage* page, const IRECT&) {
          auto* descriptionControl = new IMultiLineTextControl(IRECT(), descriptor.description, oscillatorTabDescriptionText, COLOR_TRANSPARENT);
          descriptionControl->SetIgnoreMouse(true);
          descriptionControl->DisablePrompt(true);

          auto* restoreButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Restore", oscillatorRestoreButtonStyle, true, false);
          restoreButton->SetAnimationEndActionFunction(
            [oscillatorSliderControls, editorCompoundPreset, selectedEditorMidiNote, refreshOscillatorTabs, sendOscillatorParameterToDSP, descriptor](IControl* caller) {
              if(!caller)
                return;

              const int selectedMidiNote = *selectedEditorMidiNote;
              if(selectedMidiNote < CompoundPreset::kMinMidiNote || selectedMidiNote > CompoundPreset::kMaxMidiNote)
                return;

              if(!editorCompoundPreset->HasKeyNotePreset(selectedMidiNote))
                return;

              auto* control = (*oscillatorSliderControls)[static_cast<std::size_t>(descriptor.parameter)];
              if(!control || !control->HasRestoreStateForMidiNote(selectedMidiNote))
                return;

              const auto& restoreState = control->GetRestoreState();
              for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
              {
                const double value = std::clamp(
                  restoreState[static_cast<std::size_t>(oscillatorIndex)],
                  descriptor.range.min,
                  descriptor.range.max);
                const bool updated = editorCompoundPreset->SetKeyNoteOscillatorParameter(
                  selectedMidiNote,
                  oscillatorIndex,
                  descriptor.parameter,
                  value);
                if(!updated)
                  return;

                sendOscillatorParameterToDSP(caller, selectedMidiNote, oscillatorIndex, descriptor.parameter, value);
              }

              if(*refreshOscillatorTabs)
                (*refreshOscillatorTabs)();
            });

          auto* control = new OscillatorSliderControl(IRECT(), "", levelSliderStyle, EDirection::Vertical);
          OscillatorSliderControl::Config config;
          config.range = descriptor.range;
          control->SetConfig(config);
          control->SetOnOscillatorValueChanged(
            [control, editorCompoundPreset, selectedEditorMidiNote, refreshOscillatorTabs, sendOscillatorParameterToDSP, descriptor](int oscillatorIndex, double value) {
              if(oscillatorIndex < 0 || oscillatorIndex >= SimplePreset::kNumOscillators)
                return;

              const int selectedMidiNote = *selectedEditorMidiNote;
              const double clampedValue = std::clamp(value, descriptor.range.min, descriptor.range.max);
              const bool updated = editorCompoundPreset->SetKeyNoteOscillatorParameter(
                selectedMidiNote,
                oscillatorIndex,
                descriptor.parameter,
                clampedValue);
              if(!updated)
                return;

              sendOscillatorParameterToDSP(control, selectedMidiNote, oscillatorIndex, descriptor.parameter, clampedValue);

              if(*refreshOscillatorTabs)
                (*refreshOscillatorTabs)();
            });

          page->AddChildControl(descriptionControl);
          page->AddChildControl(restoreButton);
          page->AddChildControl(control);
          (*oscillatorSliderControls)[static_cast<std::size_t>(descriptor.parameter)] = control;
          (*restoreButtons)[static_cast<std::size_t>(descriptor.parameter)] = restoreButton;
          if(*refreshOscillatorTabs)
            (*refreshOscillatorTabs)();
        },
        oscillatorTabResizeFunc,
        [oscillatorSliderControls, selectedEditorMidiNote, refreshOscillatorTabs, descriptor](bool isVisible) {
          auto* control = (*oscillatorSliderControls)[static_cast<std::size_t>(descriptor.parameter)];
          if(!control)
            return;

          if(isVisible)
          {
            const int selectedMidiNote = *selectedEditorMidiNote;
            if(selectedMidiNote >= CompoundPreset::kMinMidiNote && selectedMidiNote <= CompoundPreset::kMaxMidiNote)
              control->CaptureRestoreState(selectedMidiNote);
            else
              control->ClearRestoreState();
          }
          else
            control->ClearRestoreState();

          if(*refreshOscillatorTabs)
            (*refreshOscillatorTabs)();
        });
  };

  pGraphics->AttachControl(new IVMeterControl<1>(breathMeterBounds, "", meterStyle), breathMeterTag);
  pGraphics->AttachControl(new HarmonicVisualizerControl(harmonicVisualizerBounds), harmonicVisualizerTag);
  pGraphics->AttachControl(new IVTabbedPagesControl(presetEditorTabsBounds,
    {
      {oscillatorTabDescriptors[0].title, makeOscillatorTabPage(oscillatorTabDescriptors[0])},
      {oscillatorTabDescriptors[1].title, makeOscillatorTabPage(oscillatorTabDescriptors[1])},
      {oscillatorTabDescriptors[2].title, makeOscillatorTabPage(oscillatorTabDescriptors[2])},
      {oscillatorTabDescriptors[3].title, makeOscillatorTabPage(oscillatorTabDescriptors[3])},
      {oscillatorTabDescriptors[4].title, makeOscillatorTabPage(oscillatorTabDescriptors[4])},
      {oscillatorTabDescriptors[5].title, makeOscillatorTabPage(oscillatorTabDescriptors[5])},
      {oscillatorTabDescriptors[6].title, makeOscillatorTabPage(oscillatorTabDescriptors[6])},
      {oscillatorTabDescriptors[7].title, makeOscillatorTabPage(oscillatorTabDescriptors[7])},
      {oscillatorTabDescriptors[8].title, makeOscillatorTabPage(oscillatorTabDescriptors[8])},
      {oscillatorTabDescriptors[9].title, makeOscillatorTabPage(oscillatorTabDescriptors[9])},
      {oscillatorTabDescriptors[10].title, makeOscillatorTabPage(oscillatorTabDescriptors[10])},
      {oscillatorTabDescriptors[11].title, makeOscillatorTabPage(oscillatorTabDescriptors[11])},
    }, "", presetEditorTabsStyle, 20.f, 1.f),
    presetEditorTabsTag);
  const auto setMainPanelVizVisible = [pGraphics, breathMeterTag, harmonicVisualizerTag](bool visible) {
    if(auto* breathMeter = pGraphics->GetControlWithTag(breathMeterTag))
      breathMeter->Hide(!visible);
    if(auto* harmonicVisualizer = pGraphics->GetControlWithTag(harmonicVisualizerTag))
      harmonicVisualizer->Hide(!visible);
  };
  const auto setMainPanelEditVisible = [pGraphics, presetEditorTabsTag](bool visible) {
    if(auto* presetEditorTabs = pGraphics->GetControlWithTag(presetEditorTabsTag))
      presetEditorTabs->Hide(!visible);
  };
  const auto setKeyboardEditMode = [pGraphics, keyboardTag](bool editMode) {
    if(auto* keyboard = pGraphics->GetControlWithTag(keyboardTag))
      if(auto* presetKeyboard = keyboard->As<PresetEditorKeyboardControl>())
        presetKeyboard->SetEditMode(editMode);
  };
  const auto setMainPanelMode = [setMainPanelVizVisible, setMainPanelEditVisible, setKeyboardEditMode, isEditorEditMode, refreshEditorActionButtons](bool vizMode) {
    setMainPanelVizVisible(vizMode);
    setMainPanelEditVisible(!vizMode);
    setKeyboardEditMode(!vizMode);
    *isEditorEditMode = !vizMode;
    if(*refreshEditorActionButtons)
      (*refreshEditorActionButtons)();
  };

  // Viz/edit panel: x=4, y=428, w=180, h=84
  const IRECT mainPanelModeSwitchBounds = IRECT::MakeXYWH(73.f, 442.f, 42.f, 26.f);
  const IVStyle mainPanelModeSwitchStyle = theme::BaseStyle(false, false)
    .WithColor(kFG, kSecondaryUIValueColor)
    .WithColor(kHL, kSecondaryUIValueColor);
  pGraphics->AttachControl(
    new IVSlideSwitchControl(mainPanelModeSwitchBounds,
      [setMainPanelMode](IControl* pCaller) {
        const bool vizMode = pCaller->GetValue() < 0.5;
        setMainPanelMode(vizMode);
      }, "", mainPanelModeSwitchStyle, false, EDirection::Horizontal, 2, 0));
  const IRECT addButtonBounds = IRECT::MakeXYWH(14.f, 477.f, 75.f, 26.f);
  const IRECT deleteButtonBounds = IRECT::MakeXYWH(99.f, 477.f, 75.f, 26.f);
  const IVStyle vizEditButtonStyle = theme::BaseStyle(true, false).WithLabelText(IText(16.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle));
  auto* addButtonControl = new IVButtonControl(addButtonBounds,
    [pGraphics, keyboardTag, editorCompoundPreset, selectedEditorMidiNote, refreshOscillatorTabs, refreshEditorActionButtons, sendKeyNotePresetEditToDSP](IControl* caller) {
      if(!caller)
        return;

      const int selectedMidiNote = *selectedEditorMidiNote;
      if(selectedMidiNote < CompoundPreset::kMinMidiNote || selectedMidiNote > CompoundPreset::kMaxMidiNote)
        return;

      if(editorCompoundPreset->HasKeyNotePreset(selectedMidiNote))
        return;

      editorCompoundPreset->SetKeyNotePreset(selectedMidiNote, editorCompoundPreset->GetPresetForMidiNote(selectedMidiNote));
      sendKeyNotePresetEditToDSP(caller, preset_editor_messages::kMsgTagAddKeyNotePreset, selectedMidiNote);

      if(auto* keyboard = pGraphics->GetControlWithTag(keyboardTag))
        if(auto* presetKeyboard = keyboard->As<PresetEditorKeyboardControl>())
          presetKeyboard->SetHighlightedMidiNote(selectedMidiNote, true);

      if(*refreshOscillatorTabs)
        (*refreshOscillatorTabs)();
      if(*refreshEditorActionButtons)
        (*refreshEditorActionButtons)();
    },
    "ADD",
    vizEditButtonStyle,
    true,
    false);
  auto* deleteButtonControl = new IVButtonControl(deleteButtonBounds,
    [pGraphics, keyboardTag, editorCompoundPreset, selectedEditorMidiNote, refreshOscillatorTabs, refreshEditorActionButtons, sendKeyNotePresetEditToDSP](IControl* caller) {
      if(!caller)
        return;

      const int selectedMidiNote = *selectedEditorMidiNote;
      if(selectedMidiNote < CompoundPreset::kMinMidiNote || selectedMidiNote > CompoundPreset::kMaxMidiNote)
        return;

      const bool removed = editorCompoundPreset->RemoveKeyNotePreset(selectedMidiNote);
      if(!removed)
        return;

      sendKeyNotePresetEditToDSP(caller, preset_editor_messages::kMsgTagRemoveKeyNotePreset, selectedMidiNote);

      if(auto* keyboard = pGraphics->GetControlWithTag(keyboardTag))
        if(auto* presetKeyboard = keyboard->As<PresetEditorKeyboardControl>())
          presetKeyboard->SetHighlightedMidiNote(selectedMidiNote, false);

      if(*refreshOscillatorTabs)
        (*refreshOscillatorTabs)();
      if(*refreshEditorActionButtons)
        (*refreshEditorActionButtons)();
    },
    "DELETE",
    vizEditButtonStyle,
    true,
    false);
  pGraphics->AttachControl(addButtonControl);
  pGraphics->AttachControl(deleteButtonControl);
  *addButton = addButtonControl;
  *deleteButton = deleteButtonControl;

  // Keyboard panel: x=4, y=514, w=1082, h=130
  const IRECT wheelsBounds = IRECT::MakeXYWH(6.f, 522.f, 35.f, 114.f);
  const IRECT keyboardBounds = IRECT::MakeXYWH(40.f, 522.f, 1038.f, 114.f);
  auto* keyboardControl = new PresetEditorKeyboardControl(keyboardBounds, 21, 108);
  keyboardControl->SetOnSelectedMidiNoteChanged([selectedEditorMidiNote, refreshOscillatorTabs, refreshEditorActionButtons](int midiNote) {
    *selectedEditorMidiNote = midiNote;
    if(*refreshOscillatorTabs)
      (*refreshOscillatorTabs)();
    if(*refreshEditorActionButtons)
      (*refreshEditorActionButtons)();
  });
  keyboardControl->SetHighlightedMidiNotes(presets::kBrassCompoundPresetKeyNotes);
  keyboardControl->SetSelectedMidiNote(*selectedEditorMidiNote);
  pGraphics->AttachControl(new IWheelControl(wheelsBounds), benderTag);
  pGraphics->AttachControl(keyboardControl, keyboardTag);
  if(*refreshOscillatorTabs)
    (*refreshOscillatorTabs)();
  if(*refreshEditorActionButtons)
    (*refreshEditorActionButtons)();
  setMainPanelMode(true);

  // Envelope panel: x=186, y=428, w=212, h=84
  const IRECT attackKnobBounds = IRECT::MakeMidXYWH(216.f, 480.5f, knob_size, knob_size);
  const IRECT attackLabelBounds = IRECT::MakeXYWH(245.f, 468.f, 70.f, 12.f);
  const IRECT attackValueBounds = IRECT::MakeXYWH(245.f, 481.f, 70.f, 12.f);
  const IRECT releaseKnobBounds = IRECT::MakeMidXYWH(312.f, 480.5f, knob_size, knob_size);
  const IRECT releaseLabelBounds = IRECT::MakeXYWH(343.f, 468.f, 70.f, 12.f);
  const IRECT releaseValueBounds = IRECT::MakeXYWH(343.f, 481.f, 70.f, 12.f);
  auto* attackValueControl = new ICaptionControl(attackValueBounds, globalModifierParamIdxs[0], compactValueText, COLOR_TRANSPARENT, true);
  auto* releaseValueControl = new ICaptionControl(releaseValueBounds, globalModifierParamIdxs[5], compactValueText, COLOR_TRANSPARENT, true);
  attackValueControl->SetIgnoreMouse(true); attackValueControl->DisablePrompt(true);
  releaseValueControl->SetIgnoreMouse(true); releaseValueControl->DisablePrompt(true);
  pGraphics->AttachControl(attackValueControl);
  pGraphics->AttachControl(releaseValueControl);
  attachKnob(attackKnobBounds, globalModifierParamIdxs[0]);
  attachKnob(releaseKnobBounds, globalModifierParamIdxs[5]);
  pGraphics->AttachControl(new ITextControl(attackLabelBounds, "Attack", compactLabelText));
  pGraphics->AttachControl(new ITextControl(releaseLabelBounds, "Release", compactLabelText));

  // Pitch panel: x=400, y=428, w=262, h=84
  const IRECT pitchKnobBounds = IRECT::MakeMidXYWH(431.f, 480.5f, knob_size, knob_size);
  const IRECT pitchLabelBounds = IRECT::MakeXYWH(464.f, 468.f, 70.f, 12.f);
  const IRECT pitchValueBounds = IRECT::MakeXYWH(464.f, 481.f, 70.f, 12.f);
  const IRECT portamentoSliderBounds = IRECT::MakeXYWH(524.f, 474.f, 140.f, 30.f);
  const IRECT portamentoMinValueBounds = IRECT::MakeXYWH(527.5f, 470.f, 50.f, 20.f);
  const IRECT portamentoMaxValueBounds = IRECT::MakeXYWH(611.f, 489.f, 50.f, 20.f);
  const IRECT portamentoLabelBounds = IRECT::MakeXYWH(532.5f, 455.f, 70.f, 12.f);
  auto* pitchValueControl = new ICaptionControl(pitchValueBounds, globalModifierParamIdxs[1], compactValueText, COLOR_TRANSPARENT, true);
  auto* portamentoMinValueControl = new ICaptionControl(portamentoMinValueBounds, portamentoAtCC5MinParamIdx, portamentoValueText, COLOR_TRANSPARENT, true);
  auto* portamentoMaxValueControl = new ICaptionControl(portamentoMaxValueBounds, portamentoAtCC5MaxParamIdx, portamentoValueText, COLOR_TRANSPARENT, true);
  pitchValueControl->SetIgnoreMouse(true); pitchValueControl->DisablePrompt(true);
  portamentoMinValueControl->SetIgnoreMouse(true); portamentoMinValueControl->DisablePrompt(true);
  portamentoMaxValueControl->SetIgnoreMouse(true); portamentoMaxValueControl->DisablePrompt(true);
  pGraphics->AttachControl(pitchValueControl);
  pGraphics->AttachControl(portamentoMinValueControl);
  pGraphics->AttachControl(portamentoMaxValueControl);
  attachKnob(pitchKnobBounds, globalModifierParamIdxs[1]);
    const IVStyle portamentoRangeSliderStyle = knobStyle
    .WithColor(kFG, kSecondaryUIValueColor)
    .WithColor(kHL, kSecondaryUIValueColor);
  pGraphics->AttachControl(new IVRangeSliderControl( portamentoSliderBounds, {portamentoAtCC5MinParamIdx, portamentoAtCC5MaxParamIdx}, "", portamentoRangeSliderStyle, EDirection::Horizontal, true, 9.f, 3.f));
  pGraphics->AttachControl(new ITextControl(pitchLabelBounds, "Pitch", compactLabelText));
  pGraphics->AttachControl(new ITextControl(portamentoLabelBounds, "Portamento", compactLabelText));

  // Blip guard panel: x=664, y=428, w=180, h=84
  // TODO: add controls for blip guard settings here once implemented

  // Variation panel: x=846, y=66, w=240, h=222
  const IRECT intVarAmtKnobBounds = IRECT::MakeMidXYWH(923.f, 141.5f, knob_size, knob_size);
  const IRECT intVarAmtValueBounds = IRECT::MakeXYWH(951.f, 134.f, 70.f, 12.f);
  const IRECT intVarRateKnobBounds = IRECT::MakeMidXYWH(1018.f, 141.5f, knob_size, knob_size);
  const IRECT intVarRateValueBounds = IRECT::MakeXYWH(1046.f, 134.f, 70.f, 12.f);
  const IRECT panVarAmtKnobBounds = IRECT::MakeMidXYWH(923.f, 195.5f, knob_size, knob_size);
  const IRECT panVarAmtValueBounds = IRECT::MakeXYWH(951.f, 188.f, 70.f, 12.f);
  const IRECT panVarRateKnobBounds = IRECT::MakeMidXYWH(1018.f, 195.5f, knob_size, knob_size);
  const IRECT panVarRateValueBounds = IRECT::MakeXYWH(1046.f, 188.f, 70.f, 12.f);
  const IRECT pitchVarAmtKnobBounds = IRECT::MakeMidXYWH(923.f, 249.5f, knob_size, knob_size);
  const IRECT pitchVarAmtValueBounds = IRECT::MakeXYWH(951.f, 242.f, 70.f, 12.f);
  const IRECT pitchVarRateKnobBounds = IRECT::MakeMidXYWH(1018.f, 249.5f, knob_size, knob_size);
  const IRECT pitchVarRateValueBounds = IRECT::MakeXYWH(1046.f, 242.f, 70.f, 12.f);
  auto* intVarAmtValueControl = new ICaptionControl(intVarAmtValueBounds, globalModifierParamIdxs[2], compactValueText, COLOR_TRANSPARENT, true);
  auto* intVarRateValueControl = new ICaptionControl(intVarRateValueBounds, globalModifierParamIdxs[7], compactValueText, COLOR_TRANSPARENT, true);
  auto* panVarAmtValueControl = new ICaptionControl(panVarAmtValueBounds, globalModifierParamIdxs[4], compactValueText, COLOR_TRANSPARENT, true);
  auto* panVarRateValueControl = new ICaptionControl(panVarRateValueBounds, globalModifierParamIdxs[9], compactValueText, COLOR_TRANSPARENT, true);
  auto* pitchVarAmtValueControl = new ICaptionControl(pitchVarAmtValueBounds, globalModifierParamIdxs[3], compactValueText, COLOR_TRANSPARENT, true);
  auto* pitchVarRateValueControl = new ICaptionControl(pitchVarRateValueBounds, globalModifierParamIdxs[8], compactValueText, COLOR_TRANSPARENT, true);
  intVarAmtValueControl->SetIgnoreMouse(true); intVarAmtValueControl->DisablePrompt(true);
  intVarRateValueControl->SetIgnoreMouse(true); intVarRateValueControl->DisablePrompt(true);
  panVarAmtValueControl->SetIgnoreMouse(true); panVarAmtValueControl->DisablePrompt(true);
  panVarRateValueControl->SetIgnoreMouse(true); panVarRateValueControl->DisablePrompt(true);
  pitchVarAmtValueControl->SetIgnoreMouse(true); pitchVarAmtValueControl->DisablePrompt(true);
  pitchVarRateValueControl->SetIgnoreMouse(true); pitchVarRateValueControl->DisablePrompt(true);
  pGraphics->AttachControl(intVarAmtValueControl);
  pGraphics->AttachControl(intVarRateValueControl);
  pGraphics->AttachControl(panVarAmtValueControl);
  pGraphics->AttachControl(panVarRateValueControl);
  pGraphics->AttachControl(pitchVarAmtValueControl);
  pGraphics->AttachControl(pitchVarRateValueControl);
  attachKnob(intVarAmtKnobBounds, globalModifierParamIdxs[2]);
  attachKnob(intVarRateKnobBounds, globalModifierParamIdxs[7]);
  attachKnob(panVarAmtKnobBounds, globalModifierParamIdxs[4]);
  attachKnob(panVarRateKnobBounds, globalModifierParamIdxs[9]);
  attachKnob(pitchVarAmtKnobBounds, globalModifierParamIdxs[3]);
  attachKnob(pitchVarRateKnobBounds, globalModifierParamIdxs[8]);

  // Output panel: x=846, y=290, w=240, h=84
  const IRECT panKnobBounds = IRECT::MakeMidXYWH(886.f, 342.5f, knob_size, knob_size);
  const IRECT panLabelBounds = IRECT::MakeXYWH(918.f, 330.f, 50.f, 12.f);
  const IRECT panValueBounds = IRECT::MakeXYWH(918.f, 343.f, 50.f, 12.f);
  const IRECT gainKnobBounds = IRECT::MakeMidXYWH(992.f, 342.5f, knob_size, knob_size);
  const IRECT gainLabelBounds = IRECT::MakeXYWH(1024.f, 330.f, 50.f, 12.f);
  const IRECT gainValueBounds = IRECT::MakeXYWH(1024.f, 343.f, 50.f, 12.f);
  auto* panValueControl = new ICaptionControl(panValueBounds, globalModifierParamIdxs[6], compactValueText, COLOR_TRANSPARENT, true);
  auto* gainValueControl = new ICaptionControl(gainValueBounds, gainParamIdx, compactValueText, COLOR_TRANSPARENT, true);
  panValueControl->SetIgnoreMouse(true); panValueControl->DisablePrompt(true);
  gainValueControl->SetIgnoreMouse(true); gainValueControl->DisablePrompt(true);
  pGraphics->AttachControl(gainValueControl);
  pGraphics->AttachControl(panValueControl);
  attachKnob(gainKnobBounds, gainParamIdx);
  attachKnob(panKnobBounds, globalModifierParamIdxs[6]);
  pGraphics->AttachControl(new ITextControl(gainLabelBounds, "Level", compactLabelText));
  pGraphics->AttachControl(new ITextControl(panLabelBounds, "Pan", compactLabelText));

  // Effects panel: x=846, y=376, w=240, h=136
  // TODO: add controls for effects settings here once implemented

}

inline void HandleQwertyMidi(IGraphics* pGraphics, int keyboardTag, int& lastQwertyMIDINote, const IMidiMsg& msg)
{
  auto* keyboard = pGraphics->GetControlWithTag(keyboardTag)->As<IVKeyboardControl>();
  const int note = msg.NoteNumber();
  const bool noteOn = (msg.StatusMsg() == IMidiMsg::kNoteOn) && (msg.Velocity() > 0);

  if(noteOn)
  {
    if(lastQwertyMIDINote >= 0 && lastQwertyMIDINote != note)
      keyboard->SetNoteFromMidi(lastQwertyMIDINote, false);

    keyboard->SetNoteFromMidi(note, true);
    lastQwertyMIDINote = note;
  }
  else if(note == lastQwertyMIDINote)
  {
    keyboard->SetNoteFromMidi(note, false);
    lastQwertyMIDINote = -1;
  }
}
} // namespace plugin_ui

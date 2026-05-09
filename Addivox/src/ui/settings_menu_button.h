#pragma once

#include "../midi/breath_control.h"
#include "IControls.h"
#include "colour.h"
#include "editor_messages.h"
#include "theme.h"

#if defined OS_IOS
#include "../ios/audio_midi_settings_ios.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

namespace layout {
class SettingsMenuButton final : public IVButtonControl {
public:
  SettingsMenuButton(const IRECT& bounds, const ISVG& gearIcon, const std::shared_ptr<BreathCCSource>& breathCCSource,
                     const std::shared_ptr<int>& pitchBendRange, const std::shared_ptr<bool>& harmonicVisualizerEnabled)
      : IVButtonControl(bounds, EmptyClickActionFunc, "", theme::AboutIconButtonStyle(), false, false, EVShape::Ellipse), mGearIcon(gearIcon),
        mBreathCCSource(breathCCSource), mPitchBendRange(pitchBendRange), mHarmonicVisualizerEnabled(harmonicVisualizerEnabled) {
    SetTooltip("Settings");
    SetActionFunction([this](IControl* caller) {
      if (caller) {
        caller->SetValue(0.);
        caller->SetDirty(false);
      }

      OpenMenu();
    });
  }

  void DrawWidget(IGraphics& g) override {
    const bool pressed = GetValue() > 0.5;
    DrawPressableShape(g, EVShape::Ellipse, mWidgetBounds, pressed, mMouseIsOver, IsDisabled());

    IColor iconColor = colour::ui::kValueText;
    if (IsDisabled()) iconColor = GetColor(kFR);
    else if (pressed || mMouseIsOver)
      iconColor = GetColor(kX1);
    g.DrawSVG(mGearIcon, mWidgetBounds.GetPadded(-7.f), &mBlend, nullptr, &iconColor);
  }

  void OnPopupMenuSelection(IPopupMenu* selectedMenu, int valIdx) override {
    if (selectedMenu && selectedMenu->GetChosenItem()) {
      const char* selectedText = selectedMenu->GetChosenItem()->GetText();
      if (selectedText && std::strcmp(selectedText, kVisualizerEnabledMenuLabel) == 0) {
        const bool enabled = !(mHarmonicVisualizerEnabled && *mHarmonicVisualizerEnabled);
        if (mHarmonicVisualizerEnabled) *mHarmonicVisualizerEnabled = enabled;

        if (auto* delegate = GetDelegate()) {
          const editor_messages::SetHarmonicVisualizerEnabledPayload payload{enabled ? 1 : 0};
          delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagSetHarmonicVisualizerEnabled, GetTag(), sizeof(payload), &payload);
        }
      } else if (selectedText && std::strcmp(selectedText, kResetToDefaultsMenuLabel) == 0) {
        if (auto* delegate = GetDelegate()) {
          delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagResetStandaloneStateToDefaults, GetTag());
        }
      }
#if defined APP_API && !defined OS_IOS
      else if (selectedText && std::strcmp(selectedText, kMacAudioMidiSettingsMenuLabel) == 0) {
        if (auto* delegate = GetDelegate()) {
          delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagOpenAudioMidiSettings, GetTag());
        }
      }
#endif
      else {
        BreathCCSource selectedSource = kDefaultBreathCCSource;
        if (TryParseBreathCCSourceMenuLabel(selectedText, selectedSource)) {
          if (mBreathCCSource) *mBreathCCSource = selectedSource;

          if (auto* delegate = GetDelegate()) {
            const editor_messages::SetBreathCCSourcePayload payload{static_cast<int>(selectedSource)};
            delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagSetBreathCCSource, GetTag(), sizeof(payload), &payload);
          }
        } else {
          int selectedPitchBendRange = 2;
          if (TryParsePitchBendRangeMenuLabel(selectedText, selectedPitchBendRange)) {
            if (mPitchBendRange) *mPitchBendRange = selectedPitchBendRange;

            if (auto* delegate = GetDelegate()) {
              const editor_messages::SetPitchBendRangePayload payload{selectedPitchBendRange};
              delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagSetPitchBendRange, GetTag(), sizeof(payload), &payload);
            }
          }
        }
      }
    }

    IControl::OnPopupMenuSelection(selectedMenu, valIdx);
  }

private:
  void OpenMenu() {
    auto* ui = GetUI();
    if (!ui) return;

    BuildMenu();
    ui->CreatePopupMenu(*this, mMenu, mRECT);
  }

  void BuildMenu() {
    mMenu.Clear();
#if defined OS_IOS
    mMenu.SetFunction([this](IPopupMenu* selectedMenu) { HandleIOSAudioSettingsSelection(selectedMenu); });
#else
    mMenu.SetFunction(nullptr);
#endif

    const int visualizerFlags = (mHarmonicVisualizerEnabled && *mHarmonicVisualizerEnabled) ? IPopupMenu::Item::kChecked : 0;
    mMenu.AddItem(kVisualizerEnabledMenuLabel, -1, visualizerFlags);
    mMenu.AddSeparator();

    auto* breathMenu = new IPopupMenu("Breath CC");
    const BreathCCSource currentSource = mBreathCCSource ? *mBreathCCSource : kDefaultBreathCCSource;
    for (const BreathCCSource source : kBreathCCSources) {
      const int flags = (source == currentSource) ? IPopupMenu::Item::kChecked : 0;
      breathMenu->AddItem(GetBreathCCSourceMenuLabel(source), -1, flags);
    }

    mMenu.AddItem("Breath CC", breathMenu);

    auto* pitchBendRangeMenu = new IPopupMenu("Pitch Bend Range");
    const int currentPitchBendRange = mPitchBendRange ? *mPitchBendRange : 2;
    for (const PitchBendRangeMenuItem& item : kPitchBendRangeMenuItems) {
      const int flags = (item.semitones == currentPitchBendRange) ? IPopupMenu::Item::kChecked : 0;
      pitchBendRangeMenu->AddItem(item.label, -1, flags);
    }

    mMenu.AddItem("Pitch Bend Range", pitchBendRangeMenu);
    mMenu.AddSeparator();
#if defined APP_API && !defined OS_IOS
    mMenu.AddItem(kMacAudioMidiSettingsMenuLabel);
#endif
#if defined OS_IOS
    if (addivox_standalone::IsAudioMidiSettingsMenuAvailable()) {
      auto* audioMenu = mMenu.AddItem(kIOSAudioMidiSettingsMenuLabel, new IPopupMenu(kIOSAudioMidiSettingsMenuLabel))->GetSubmenu();
      auto* sampleRateMenu = new IPopupMenu("Sample Rate");
      const int currentSampleRate = addivox_standalone::GetPreferredSampleRate();
      for (const int sampleRate : addivox_standalone::kIOSAudioSettingsSampleRates) {
        char label[16];
        std::snprintf(label, sizeof(label), "%d", sampleRate);
        const int flags = (sampleRate == currentSampleRate) ? IPopupMenu::Item::kChecked : 0;
        sampleRateMenu->AddItem(label, -1, flags);
      }

      auto* bufferSizeMenu = new IPopupMenu("Buffer Size");
      const int currentBufferSize = addivox_standalone::GetPreferredBufferSize();
      for (const int bufferSize : addivox_standalone::kIOSAudioSettingsBufferSizes) {
        char label[16];
        std::snprintf(label, sizeof(label), "%d", bufferSize);
        const int flags = (bufferSize == currentBufferSize) ? IPopupMenu::Item::kChecked : 0;
        bufferSizeMenu->AddItem(label, -1, flags);
      }

      audioMenu->AddItem("Sample Rate", sampleRateMenu);
      audioMenu->AddItem("Buffer Size", bufferSizeMenu);
      mMenu.AddSeparator();
    }
#endif
    mMenu.AddItem(kResetToDefaultsMenuLabel);
  }

#if defined OS_IOS
  bool HandleIOSAudioSettingsSelection(IPopupMenu* selectedMenu) {
    if (!selectedMenu || !selectedMenu->GetChosenItem()) return false;

    const char* selectedText = selectedMenu->GetChosenItem()->GetText();
    if (!selectedText) return false;

    char* end = nullptr;
    const long value = std::strtol(selectedText, &end, 10);
    if (end == selectedText || *end != '\0') return false;

    for (const int sampleRate : addivox_standalone::kIOSAudioSettingsSampleRates) {
      if (value == sampleRate) return addivox_standalone::SetPreferredSampleRate(static_cast<int>(value));
    }

    for (const int bufferSize : addivox_standalone::kIOSAudioSettingsBufferSizes) {
      if (value == bufferSize) return addivox_standalone::SetPreferredBufferSize(static_cast<int>(value));
    }

    return false;
  }
#endif

  IPopupMenu mMenu{"Settings"};
  ISVG mGearIcon;
  std::shared_ptr<BreathCCSource> mBreathCCSource;
  std::shared_ptr<int> mPitchBendRange;
  std::shared_ptr<bool> mHarmonicVisualizerEnabled;

  struct PitchBendRangeMenuItem {
    const char* label;
    int semitones;
  };

  static constexpr const char* kVisualizerEnabledMenuLabel = "Visualizer Enabled";
  static constexpr PitchBendRangeMenuItem kPitchBendRangeMenuItems[] = {{"1 semitone", 1}, {"2 semitones", 2}, {"Fifth", 7}, {"Octave", 12}};
#if defined APP_API && !defined OS_IOS
  static constexpr const char* kMacAudioMidiSettingsMenuLabel = "Audio & MIDI Settings...";
#endif
#if defined OS_IOS
  static constexpr const char* kIOSAudioMidiSettingsMenuLabel = "Audio & MIDI Settings";
#endif
  static constexpr const char* kResetToDefaultsMenuLabel = "Reset Synth Settings to Defaults";

  static bool TryParsePitchBendRangeMenuLabel(const char* label, int& semitones) {
    if (!label) return false;

    for (const PitchBendRangeMenuItem& item : kPitchBendRangeMenuItems) {
      if (std::strcmp(label, item.label) == 0) {
        semitones = item.semitones;
        return true;
      }
    }

    return false;
  }
};
} // namespace layout
} // namespace plugin_ui

#pragma once

#include "../../settings/oscillator.h"
#include "../action_selection_control.h"
#include "../control_utils.h"
#include "../editor_messages.h"
#include "../editor_state.h"
#include "../eq_editor.h"
#include "../help_text.h"
#include "../keyboard_control.h"
#include "../oscillator_slider_control.h"
#include "../theme.h"
#include "IControls.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

namespace editor {
using OscillatorParameter = OscillatorSettings::Parameter;
using OscillatorParameterValues = CompoundPatch::OscillatorParameterValues;
using SliderRange = OscillatorSliderControl::ValueRange;
using EditorStyles = theme::EditorStyles;

inline constexpr float        kEditorControlHeight =  20.f;
inline constexpr float       kHarmonicTabLeftInset = 104.f;
inline constexpr float       kHarmonicTabSideInset =   8.f;
inline constexpr float     kHarmonicTabLabelHeight =  14.f;
inline constexpr float      kHarmonicTabControlGap =  10.f;
inline constexpr float kHarmonicTabScopeSectionGap =   6.f;
inline constexpr float       kHarmonicTabBottomPad =   8.f;
inline constexpr float   kHarmonicTabXRangeHalfGap =   6.f;
inline constexpr float  kHarmonicTabToggleLabelGap =   8.f;
inline constexpr float kHarmonicTabScopeRightInset =   4.f;
inline constexpr float        kHarmonicTabLabelGap =   0.f;
inline constexpr float        kHarmonicTabScopeGap =   0.f;
inline constexpr float           kTabButtonHalfGap =   3.f;
inline constexpr int        kHarmonicTabChildCount =  18;

struct OscillatorTabDescriptor {
  const char* title;
  const char* panelTitle;
  OscillatorParameter parameter;
  SliderRange range;
  const char* description;
};

struct HarmonicTabLayout {
  IRECT xRangeLabelBounds{};
  IRECT xRangeMinBounds{};
  IRECT xRangeMaxBounds{};
  IRECT yTransformLabelBounds{};
  IRECT yTransformBounds{};
  IRECT editModeLabelBounds{};
  IRECT editModeBounds{};
  IRECT scopeBounds{};
  IRECT setShapeLabelBounds{};
  IRECT setShapeBounds{};
  IRECT actionsLabelBounds{};
  IRECT actionsBounds{};
  IRECT allKeyNotesToggleBounds{};
  IRECT allKeyNotesLabelBounds{};
  IRECT restoreButtonBounds{};
  IRECT addButtonBounds{};
  IRECT deleteButtonBounds{};
  IRECT sliderBounds{};
};

// IVTabbedPagesControl stores pages in a std::map<const char*, ...>, so
// ordering follows pointer values rather than insertion order. Keeping all
// titles in one contiguous buffer makes the pointer order deterministic and
// match the desired tab order.
inline constexpr char kOscillatorTabTitleStorage[] = "EQ\0"
                                                     "Level\0"
                                                     "Breath\0"
                                                     "Attack\0"
                                                     "Release\0"
                                                     "Pitch\0"
                                                     "Pan\0"
                                                     "LvlVarAmt\0"
                                                     "LvlVarRate\0"
                                                     "PanVarAmt\0"
                                                     "PanVarRate\0"
                                                     "PchVarAmt\0"
                                                     "PchVarRate\0";

inline constexpr const char* kEqTabTitle = kOscillatorTabTitleStorage + 0;

inline constexpr std::array<const char*, OscillatorSettings::kNumParameters> kOscillatorTabTitles{{
    kOscillatorTabTitleStorage + 3,
    kOscillatorTabTitleStorage + 9,
    kOscillatorTabTitleStorage + 16,
    kOscillatorTabTitleStorage + 23,
    kOscillatorTabTitleStorage + 31,
    kOscillatorTabTitleStorage + 37,
    kOscillatorTabTitleStorage + 41,
    kOscillatorTabTitleStorage + 51,
    kOscillatorTabTitleStorage + 83,
    kOscillatorTabTitleStorage + 93,
    kOscillatorTabTitleStorage + 62,
    kOscillatorTabTitleStorage + 72,
}};

inline constexpr const char*     kActionScaleUp = "scale up";
inline constexpr const char*   kActionScaleDown = "scale down";
inline constexpr const char*   kActionTowardMax = "toward max";
inline constexpr const char* kActionAwayFromMax = "away from max";
inline constexpr const char*      kActionBendUp = "bend up";
inline constexpr const char*    kActionBendDown = "bend down";
inline constexpr const char*     kActionShiftUp = "shift up";
inline constexpr const char*   kActionShiftDown = "shift down";
inline constexpr const char*  kActionShiftRight = "shift right";
inline constexpr const char*   kActionShiftLeft = "shift left";
inline constexpr const char*   kActionNormalize = "normalize";
inline constexpr const char*      kActionInvert = "invert";

inline constexpr const char*     kActionScaleUpMenuLabel = "scale up [Q]";
inline constexpr const char*   kActionScaleDownMenuLabel = "scale down [A]";
inline constexpr const char*   kActionTowardMaxMenuLabel = "toward max [W]";
inline constexpr const char* kActionAwayFromMaxMenuLabel = "away from max [S]";
inline constexpr const char*      kActionBendUpMenuLabel = "bend up [E]";
inline constexpr const char*    kActionBendDownMenuLabel = "bend down [D]";
inline constexpr const char*     kActionShiftUpMenuLabel = "shift up [W]";
inline constexpr const char*   kActionShiftDownMenuLabel = "shift down [S]";
inline constexpr const char*  kActionShiftRightMenuLabel = "shift right [E]";
inline constexpr const char*   kActionShiftLeftMenuLabel = "shift left [D]";
inline constexpr const char*   kActionNormalizeMenuLabel = "normalize [N]";
inline constexpr const char*      kActionInvertMenuLabel = "invert [I]";

const std::vector<OscillatorTabDescriptor>& GetOscillatorTabDescriptors();

inline const char* GetOscillatorTabDescriptionForTitle(const char* title) {
  if (!title) return "";

  if (std::strcmp(title, kEqTabTitle) == 0) return help_text::oscillator_tabs::kEq;

  for (const auto& descriptor : GetOscillatorTabDescriptors()) {
    if (std::strcmp(descriptor.title, title) == 0) return descriptor.description;
  }

  return "";
}

inline const char* GetLevelTransformLabel(EditorLevelTransform transform) {
  switch (transform) {
  case EditorLevelTransform::SquareRoot: return "square root";
  case EditorLevelTransform::PseudoLog:  return "pseudo-log";
  case EditorLevelTransform::Linear:
  default:                               return "linear";
  }
}

inline OscillatorSliderControl::ValueTransform GetSliderValueTransform(EditorLevelTransform transform) {
  switch (transform) {
  case EditorLevelTransform::SquareRoot: return OscillatorSliderControl::ValueTransform::SquareRoot;
  case EditorLevelTransform::PseudoLog:  return OscillatorSliderControl::ValueTransform::PseudoLog;
  case EditorLevelTransform::Linear:
  default:                               return OscillatorSliderControl::ValueTransform::Linear;
  }
}

inline const char* GetOscillatorEditModeLabel(EditorOscillatorEditMode mode) {
  switch (mode) {
  case EditorOscillatorEditMode::DrawLine: return "draw line";
  case EditorOscillatorEditMode::Smooth:   return "smooth";
  case EditorOscillatorEditMode::Nudge:    return "nudge";
  case EditorOscillatorEditMode::Set:
  default:                                 return "set";
  }
}

inline EditorOscillatorEditMode GetOscillatorEditModeFromLabel(const char* label) {
  if (label && std::strcmp(label, "draw line") == 0) return EditorOscillatorEditMode::DrawLine;
  if (label && std::strcmp(label, "smooth") == 0) return EditorOscillatorEditMode::Smooth;
  if (label && std::strcmp(label, "nudge") == 0) return EditorOscillatorEditMode::Nudge;

  return EditorOscillatorEditMode::Set;
}

inline int GetOscillatorEditScopeIndex(EditorOscillatorEditScope scope) {
  switch (scope) {
  case EditorOscillatorEditScope::Odd:  return 1;
  case EditorOscillatorEditScope::Even: return 2;
  case EditorOscillatorEditScope::All:
  default:                              return 0;
  }
}

inline double GetOscillatorEditScopeControlValue(EditorOscillatorEditScope scope) { return static_cast<double>(GetOscillatorEditScopeIndex(scope)) / 2.0; }

inline EditorOscillatorEditScope GetOscillatorEditScopeFromIndex(int index) {
  switch (index) {
  case 1:  return EditorOscillatorEditScope::Odd;
  case 2:  return EditorOscillatorEditScope::Even;
  case 0:
  default: return EditorOscillatorEditScope::All;
  }
}

inline bool IsOddHarmonic(int oscillatorIndex) { return (oscillatorIndex % 2) == 0; }

inline void SetDisabledState(IControl* control, bool disabled) {
  if (!control || control->IsDisabled() == disabled) return;

  control->SetDisabled(disabled);
}

inline bool AreNearlyEqual(double lhs, double rhs, double tolerance = 1.0e-9) { return std::fabs(lhs - rhs) <= tolerance; }

inline void SetControlValueSilently(IControl* control, double value, int valIdx = 0) {
  if (!control || AreNearlyEqual(control->GetValue(valIdx), value)) return;

  control->SetValue(value, valIdx);
  control->SetDirty(false, valIdx);
}

inline int RoundOscillatorRangeValue(double value) { return std::clamp(static_cast<int>(std::lround(value)), 1, SimplePatch::kNumOscillators); }

inline OscillatorParameterValues GetOscillatorParameterValues(const SimplePatch& patch, OscillatorParameter parameter) {
  OscillatorParameterValues values{};
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    values[static_cast<std::size_t>(oscillatorIndex)] = patch.GetOscillatorSettings(oscillatorIndex).GetParameter(parameter);
  }

  return values;
}

inline ActionSelectionControl* CreateYTransformControl(const std::shared_ptr<EditorLevelTransform>& transform, OscillatorSliderControl* sliderControl,
                                                       const EditorStyles& styles) {
  auto* control = new ActionSelectionControl(IRECT(), GetLevelTransformLabel(*transform), {"linear", "square root", "pseudo-log"}, styles.utilityDropdownText,
                                             styles.darkTab, true);
  control->SetTooltip(help_text::oscillator_tabs::kYTransform);
  control->SetOnSelection([transform, sliderControl](const char* selectedText) {
    if (!selectedText) return;

    if (std::strcmp(selectedText, "square root") == 0) *transform = EditorLevelTransform::SquareRoot;
    else if (std::strcmp(selectedText, "pseudo-log") == 0)
      *transform = EditorLevelTransform::PseudoLog;
    else
      *transform = EditorLevelTransform::Linear;

    auto config = sliderControl->GetConfig();
    config.transform = GetSliderValueTransform(*transform);
    sliderControl->SetConfig(config);
  });
  return control;
}

inline ActionSelectionControl* CreateEditModeControl(const std::shared_ptr<std::array<EditorOscillatorEditMode, OscillatorSettings::kNumParameters>>& editModes,
                                                     const OscillatorTabDescriptor& descriptor, const EditorStyles& styles) {
  const auto parameterIndex = static_cast<std::size_t>(descriptor.parameter);
  auto* control = new ActionSelectionControl(IRECT(), GetOscillatorEditModeLabel((*editModes)[parameterIndex]), {"set", "nudge", "smooth", "draw line"},
                                             styles.utilityDropdownText, styles.darkTab, true);
  control->SetTooltip(help_text::oscillator_tabs::kEditMode);
  control->SetOnSelection([editModes, parameterIndex](const char* selectedText) {
    if (!selectedText) return;

    (*editModes)[parameterIndex] = GetOscillatorEditModeFromLabel(selectedText);
  });
  return control;
}

inline IVStyle GetEditModeScopeStyle(const EditorStyles& styles) {
  return styles.utilityToggleStyle.WithValueText(styles.utilityDropdownText.WithAlign(EAlign::Near).WithVAlign(EVAlign::Middle))
      .WithColor(kBG, COLOR_TRANSPARENT)
      .WithColor(kFG, COLOR_TRANSPARENT)
      .WithColor(kPR, COLOR_TRANSPARENT)
      .WithColor(kFR, colour::ui::kControlFrame)
      .WithColor(kON, colour::ui::kValueText)
      .WithColor(kX1, colour::ui::kValueText);
}

class EditorOscillatorEditScopeControl final : public IVRadioButtonControl {
public:
  static constexpr int kScopeOptionCount = 3;

  EditorOscillatorEditScopeControl(const IRECT& bounds,
                                   const std::shared_ptr<std::array<EditorOscillatorEditScope, OscillatorSettings::kNumParameters>>& editScopes,
                                   const OscillatorTabDescriptor& descriptor, const EditorStyles& styles)
      : IVRadioButtonControl(
            bounds,
            [editScopes, parameterIndex = static_cast<std::size_t>(descriptor.parameter)](IControl* caller) {
              auto* control = caller ? caller->As<IVRadioButtonControl>() : nullptr;
              if (!control) return;

              (*editScopes)[parameterIndex] = GetOscillatorEditScopeFromIndex(control->GetSelectedIdx());
            },
            {"all", "odd", "even"}, "", GetEditModeScopeStyle(styles), EVShape::Rectangle, EDirection::Horizontal, 7.0f) {
    mButtonAreaWidth = 9.0f;
    SetTooltip(help_text::oscillator_tabs::kEditScope);
    SetControlValueSilently(this, GetOscillatorEditScopeControlValue((*editScopes)[static_cast<std::size_t>(descriptor.parameter)]));
  }

  void DrawWidget(IGraphics& g) override {
    const int selected = GetSelectedIdx();
    const auto optionLayouts = GetOptionLayouts(&g);

    for (int i = 0; i < mNumStates && i < kScopeOptionCount; ++i) {
      const bool isSelected = i == selected;
      const bool isMouseOver = mMouseOverButton == i;

      const auto& layout = optionLayouts[static_cast<std::size_t>(i)];
      IRECT buttonBounds = layout.buttonBounds;

      if (isMouseOver) g.FillRect(GetColor(kHL), buttonBounds, &mBlend);

      g.DrawRect(GetColor(kFR), buttonBounds, &mBlend, mStyle.frameThickness);

      if (isSelected) {
        const float markSize = std::max(3.f, mButtonSize * 0.45f);
        g.FillRect(GetColor(kON), buttonBounds.GetCentredInside(markSize), &mBlend);
      }

      if (mTabLabels.Get(i)) {
        g.DrawText(mStyle.valueText.WithFGColor(isSelected ? GetColor(kON) : GetColor(kX1)), mTabLabels.Get(i)->Get(), layout.textBounds, &mBlend);
      }
    }
  }

protected:
  int GetButtonForPoint(float x, float y) const override {
    const auto optionLayouts = GetOptionLayouts(GetUI());
    for (int i = 0; i < mNumStates && i < kScopeOptionCount; ++i) {
      if (optionLayouts[static_cast<std::size_t>(i)].hitBounds.Contains(x, y)) return i;
    }

    return -1;
  }

private:
  struct ScopeOptionLayout {
    IRECT buttonBounds{};
    IRECT textBounds{};
    IRECT hitBounds{};
  };

  std::array<ScopeOptionLayout, kScopeOptionCount> GetOptionLayouts(const IGraphics* graphics) const {
    std::array<ScopeOptionLayout, kScopeOptionCount> layouts{};
    if (!graphics) return layouts;

    constexpr float kContentLeftInset = 0.5f;
    constexpr float kBoxLabelGap = 2.f;
    constexpr float kInterItemGapTrim = 2.f;
    std::array<float, kScopeOptionCount> textWidths{};
    float totalTextWidth = 0.f;

    for (int i = 0; i < kScopeOptionCount; ++i) {
      if (!mTabLabels.Get(i)) continue;

      IRECT measuredTextBounds;
      graphics->MeasureText(mStyle.valueText, mTabLabels.Get(i)->Get(), measuredTextBounds);
      textWidths[static_cast<std::size_t>(i)] = measuredTextBounds.W();
      totalTextWidth += measuredTextBounds.W();
    }

    const float totalBoxWidth = mButtonSize * static_cast<float>(kScopeOptionCount);
    const float totalLabelGapWidth = kBoxLabelGap * static_cast<float>(kScopeOptionCount);
    const float contentWidth = std::max(0.f, mWidgetBounds.W() - kContentLeftInset);
    const float freeWidth = std::max(0.f, contentWidth - (totalBoxWidth + totalTextWidth + totalLabelGapWidth));
    const float interItemGap = std::max(0.f, (freeWidth / static_cast<float>(kScopeOptionCount - 1)) - kInterItemGapTrim);

    float x = mWidgetBounds.L + kContentLeftInset;
    const float boxTop = mWidgetBounds.MH() - (mButtonSize * 0.5f);

    for (int i = 0; i < kScopeOptionCount; ++i) {
      auto& layout = layouts[static_cast<std::size_t>(i)];
      layout.buttonBounds = IRECT(x, boxTop, x + mButtonSize, boxTop + mButtonSize);

      const float textLeft = layout.buttonBounds.R + kBoxLabelGap;
      const float textRight = textLeft + textWidths[static_cast<std::size_t>(i)];
      layout.textBounds = IRECT(textLeft, mWidgetBounds.T, textRight, mWidgetBounds.B);
      layout.hitBounds = IRECT(layout.buttonBounds.L, mWidgetBounds.T, layout.textBounds.R, mWidgetBounds.B);

      x = textRight;
      if (i + 1 < kScopeOptionCount) x += interItemGap;
    }

    return layouts;
  }
};

inline EditorOscillatorEditScopeControl*
CreateEditModeScopeControl(const std::shared_ptr<std::array<EditorOscillatorEditScope, OscillatorSettings::kNumParameters>>& editScopes,
                           const OscillatorTabDescriptor& descriptor, const EditorStyles& styles) {
  return new EditorOscillatorEditScopeControl(IRECT(), editScopes, descriptor, styles);
}

inline ITextControl* CreateUtilityLabelControl(const char* text, const EditorStyles& styles, const char* tooltip = nullptr) {
  auto* control = MakePassiveControl(new ITextControl(IRECT(), text, styles.utilityLabelText, COLOR_TRANSPARENT));
  if (tooltip && tooltip[0] != '\0') control->SetTooltip(tooltip);
  return control;
}

inline ITextControl* CreateEditModeLabelControl(const EditorStyles& styles) {
  auto* control = CreateUtilityLabelControl("Edit mode:", styles);
  control->SetTooltip(help_text::oscillator_tabs::kEditMode);
  return control;
}

inline std::size_t GetAttackReleaseTabIndex(OscillatorParameter parameter) { return parameter == OscillatorParameter::release ? 1u : 0u; }

inline bool IsVariationParameter(OscillatorParameter parameter) {
  const int index = static_cast<int>(parameter);
  return index >= static_cast<int>(OscillatorParameter::level_variation_amplitude) && index <= static_cast<int>(OscillatorParameter::pan_variation_rate);
}

inline bool UsesHarmonicOscillatorTabLayout(OscillatorParameter parameter) {
  return parameter == OscillatorParameter::level || parameter == OscillatorParameter::breath_power || parameter == OscillatorParameter::attack ||
         parameter == OscillatorParameter::release || parameter == OscillatorParameter::pitch || parameter == OscillatorParameter::pan ||
         IsVariationParameter(parameter);
}

inline bool MatchesActionLabel(const char* selectedText, const char* actionName) {
  if (!selectedText || !actionName) return false;

  const std::size_t actionNameLength = std::strlen(actionName);
  if (std::strncmp(selectedText, actionName, actionNameLength) != 0) return false;

  const char suffixStart = selectedText[actionNameLength];
  return suffixStart == '\0' || (suffixStart == ' ' && selectedText[actionNameLength + 1] == '[');
}

inline const char* GetEditorActionShortcutActionName(OscillatorParameter parameter, int keyVK) {
  switch (keyVK) {
  case kVK_Q: return kActionScaleUp;
  case kVK_A: return kActionScaleDown;
  case kVK_W: return (parameter == OscillatorParameter::pitch || parameter == OscillatorParameter::pan) ? kActionShiftUp : kActionTowardMax;
  case kVK_S: return (parameter == OscillatorParameter::pitch || parameter == OscillatorParameter::pan) ? kActionShiftDown : kActionAwayFromMax;
  case kVK_E: return (parameter == OscillatorParameter::pitch || parameter == OscillatorParameter::pan) ? nullptr : kActionBendUp;
  case kVK_D: return (parameter == OscillatorParameter::pitch || parameter == OscillatorParameter::pan) ? nullptr : kActionBendDown;
  case kVK_N: return parameter == OscillatorParameter::level ? kActionNormalize : nullptr;
  case kVK_I: return (parameter == OscillatorParameter::pitch || parameter == OscillatorParameter::pan) ? kActionInvert : nullptr;
  default:    return nullptr;
  }
}

inline bool IsEditorActionShortcutKey(int keyVK) {
  switch (keyVK) {
  case kVK_Q:
  case kVK_A:
  case kVK_W:
  case kVK_S:
  case kVK_E:
  case kVK_D:
  case kVK_N:
  case kVK_I: return true;
  default:    return false;
  }
}

inline int GetQwertyMidiNoteOffset(int keyVK) {
  switch (keyVK) {
  case kVK_A: return 0;
  case kVK_W: return 1;
  case kVK_S: return 2;
  case kVK_E: return 3;
  case kVK_D: return 4;
  case kVK_F: return 5;
  case kVK_T: return 6;
  case kVK_G: return 7;
  case kVK_Y: return 8;
  case kVK_H: return 9;
  case kVK_U: return 10;
  case kVK_J: return 11;
  case kVK_K: return 12;
  case kVK_O: return 13;
  case kVK_L: return 14;
  default:    return -1;
  }
}

inline bool IsQwertyMidiOctaveKey(int keyVK) { return keyVK == kVK_Z || keyVK == kVK_X; }

inline bool IsQwertyMidiKeyboardKey(int keyVK) { return GetQwertyMidiNoteOffset(keyVK) >= 0 || IsQwertyMidiOctaveKey(keyVK); }

inline bool MatchesOscillatorEditScope(EditorOscillatorEditScope editScope, int oscillatorIndex) {
  switch (editScope) {
  case EditorOscillatorEditScope::Even: return !IsOddHarmonic(oscillatorIndex);
  case EditorOscillatorEditScope::Odd:  return IsOddHarmonic(oscillatorIndex);
  case EditorOscillatorEditScope::All:
  default:                              return true;
  }
}

inline double ApplyLinearScale(double value, double scale, double minValue, double maxValue) { return std::clamp(value * scale, minValue, maxValue); }

inline double ApplyTowardMaxScale(double value, double factor, double minValue, double maxValue) {
  return std::clamp(maxValue - ((maxValue - value) * factor), minValue, maxValue);
}

inline double ApplyShiftOffset(double value, double offset, double minValue, double maxValue) { return std::clamp(value + offset, minValue, maxValue); }

inline double ApplyCurveWarp(double value, double exponent, double minValue, double maxValue) {
  if (!std::isfinite(exponent) || exponent <= 0.0 || maxValue <= minValue) return std::clamp(value, minValue, maxValue);

  const double normalizedValue = std::clamp((value - minValue) / (maxValue - minValue), 0.0, 1.0);
  const double warpedValue = std::pow(normalizedValue, exponent);
  return minValue + ((maxValue - minValue) * warpedValue);
}

inline bool ApplyStandardHarmonicAction(SimplePatch& patch, OscillatorParameter parameter, const char* actionName, double minValue, double maxValue,
                                        EditorOscillatorEditScope editScope = EditorOscillatorEditScope::All) {
  constexpr double kScaleUp = 1.01010101010101;
  constexpr double kScaleDown = 0.99;
  constexpr double kTowardMaxFactor = 0.999;
  constexpr double kAwayFromMaxFactor = 1.001001001001001;
  constexpr double kBendExponent = 1.05;

  const bool isScaleUp = MatchesActionLabel(actionName, kActionScaleUp);
  const bool isScaleDown = MatchesActionLabel(actionName, kActionScaleDown);
  const bool isTowardMax = MatchesActionLabel(actionName, kActionTowardMax);
  const bool isAwayFromMax = MatchesActionLabel(actionName, kActionAwayFromMax);
  const bool isBendUp = MatchesActionLabel(actionName, kActionBendUp);
  const bool isBendDown = MatchesActionLabel(actionName, kActionBendDown);

  if (!(isScaleUp || isScaleDown || isTowardMax || isAwayFromMax || isBendUp || isBendDown)) return false;

  double currentMinValue = maxValue;
  double currentMaxValue = minValue;
  if (isBendUp || isBendDown) {
    for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
      if (!MatchesOscillatorEditScope(editScope, oscillatorIndex)) continue;

      const double value = patch.GetOscillatorSettings(oscillatorIndex).GetParameter(parameter);
      currentMinValue = std::min(currentMinValue, value);
      currentMaxValue = std::max(currentMaxValue, value);
    }
  }

  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    if (!MatchesOscillatorEditScope(editScope, oscillatorIndex)) continue;

    const double value = patch.GetOscillatorSettings(oscillatorIndex).GetParameter(parameter);
    double updatedValue = value;

    if (isScaleUp) updatedValue = ApplyLinearScale(value, kScaleUp, minValue, maxValue);
    else if (isScaleDown)
      updatedValue = ApplyLinearScale(value, kScaleDown, minValue, maxValue);
    else if (isTowardMax)
      updatedValue = ApplyTowardMaxScale(value, kTowardMaxFactor, minValue, maxValue);
    else if (isAwayFromMax)
      updatedValue = ApplyTowardMaxScale(value, kAwayFromMaxFactor, minValue, maxValue);
    else if (isBendUp)
      updatedValue = ApplyCurveWarp(value, 1.0 / kBendExponent, currentMinValue, currentMaxValue);
    else if (isBendDown)
      updatedValue = ApplyCurveWarp(value, kBendExponent, currentMinValue, currentMaxValue);

    patch.SetOscillatorParameter(oscillatorIndex, parameter, updatedValue);
  }

  return true;
}

inline bool ApplyScaleAction(SimplePatch& patch, OscillatorParameter parameter, const char* actionName, double minValue, double maxValue,
                             EditorOscillatorEditScope editScope = EditorOscillatorEditScope::All) {
  return ApplyStandardHarmonicAction(patch, parameter, actionName, minValue, maxValue, editScope);
}

inline std::size_t GetVariationTabIndex(OscillatorParameter parameter) {
  return static_cast<std::size_t>(static_cast<int>(parameter) - static_cast<int>(OscillatorParameter::level_variation_amplitude));
}

struct EditorModelRefs {
  std::shared_ptr<CompoundPatch> compoundPatch;
  std::shared_ptr<BreathCCSource> breathCCSource;
  std::shared_ptr<bool> harmonicVisualizerEnabled;
  std::shared_ptr<int> selectedMidiNote;
  std::shared_ptr<int> selectedTabIndex;
  std::shared_ptr<bool> editMode;
  std::shared_ptr<std::array<EditorOscillatorEditMode, OscillatorSettings::kNumParameters>> oscillatorEditModes;
  std::shared_ptr<std::array<EditorOscillatorEditScope, OscillatorSettings::kNumParameters>> oscillatorEditScopes;
};

struct OscillatorViewRefs {
  std::shared_ptr<int> xRangeMin;
  std::shared_ptr<int> xRangeMax;
};

struct LevelTabRefs {
  std::shared_ptr<EditorLevelTransform> levelTransform;
  std::shared_ptr<ActionSelectionControl*> setShapeControl;
  std::shared_ptr<ActionSelectionControl*> actionsControl;
};

struct BreathTabRefs {
  std::shared_ptr<EditorLevelTransform> breathTransform;
  std::shared_ptr<ActionSelectionControl*> setShapeControl;
  std::shared_ptr<ActionSelectionControl*> actionsControl;
};

struct PitchTabRefs {
  std::shared_ptr<EditorLevelTransform> pitchTransform;
  std::shared_ptr<ActionSelectionControl*> setShapeControl;
  std::shared_ptr<ActionSelectionControl*> actionsControl;
};

struct PanTabRefs {
  std::shared_ptr<EditorLevelTransform> panTransform;
  std::shared_ptr<ActionSelectionControl*> setShapeControl;
  std::shared_ptr<ActionSelectionControl*> actionsControl;
};

struct VariationTabRefs {
  std::shared_ptr<std::array<EditorLevelTransform, 6>> transforms;
  std::shared_ptr<std::array<ActionSelectionControl*, 6>> setShapeControls;
  std::shared_ptr<std::array<ActionSelectionControl*, 6>> actionsControls;
};

struct AttackReleaseTabRefs {
  std::shared_ptr<EditorLevelTransform> attackTransform;
  std::shared_ptr<EditorLevelTransform> releaseTransform;
  std::shared_ptr<std::array<ActionSelectionControl*, 2>> setShapeControls;
  std::shared_ptr<std::array<ActionSelectionControl*, 2>> actionsControls;
};

struct EqTabRefs {
  std::shared_ptr<ActionSelectionControl*> setShapeControl;
  std::shared_ptr<ActionSelectionControl*> actionsControl;
  std::shared_ptr<IVToggleControl*> allKeyNotesToggle;
  std::shared_ptr<IVButtonControl*> restoreButton;
  std::shared_ptr<IVButtonControl*> addButton;
  std::shared_ptr<IVButtonControl*> deleteButton;
  std::shared_ptr<EqEditorControl*> editorControl;
};

struct OscillatorTabControlRefs {
  std::shared_ptr<std::array<OscillatorSliderControl*, OscillatorSettings::kNumParameters>> sliderControls;
  std::shared_ptr<std::array<IVNumberBoxControl*, OscillatorSettings::kNumParameters>> xRangeMinControls;
  std::shared_ptr<std::array<IVNumberBoxControl*, OscillatorSettings::kNumParameters>> xRangeMaxControls;
  std::shared_ptr<std::array<IVToggleControl*, OscillatorSettings::kNumParameters>> allKeyNotesToggles;
  std::shared_ptr<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>> restoreButtons;
  std::shared_ptr<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>> addButtons;
  std::shared_ptr<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>> deleteButtons;
};

struct TitleControlRefs {
  std::shared_ptr<IControl*> patchManagerControl;
};

struct EditorContext {
  int editorTabsTag = kNoTag;
  EditorModelRefs model;
  OscillatorViewRefs oscillatorView;
  LevelTabRefs levelTab;
  BreathTabRefs breathTab;
  PitchTabRefs pitchTab;
  PanTabRefs panTab;
  VariationTabRefs variationTab;
  AttackReleaseTabRefs attackReleaseTab;
  EqTabRefs eqTab;
  OscillatorTabControlRefs oscillatorTabControls;
  std::shared_ptr<KeyboardControl*> keyboardControl;
  TitleControlRefs title;

  CompoundPatch& Patch() const { return *model.compoundPatch; }

  int SelectedMidiNote() const { return *model.selectedMidiNote; }

  int SelectedTabIndex() const { return *model.selectedTabIndex; }

  void SetSelectedMidiNote(int midiNote) const { *model.selectedMidiNote = midiNote; }

  bool IsEditMode() const { return *model.editMode; }

  void SetEditMode(bool editMode) const { *model.editMode = editMode; }

  EditorOscillatorEditMode GetOscillatorEditMode(OscillatorParameter parameter) const {
    return (*model.oscillatorEditModes)[static_cast<std::size_t>(parameter)];
  }

  void SetOscillatorEditMode(OscillatorParameter parameter, EditorOscillatorEditMode editMode) const {
    (*model.oscillatorEditModes)[static_cast<std::size_t>(parameter)] = editMode;
  }

  EditorOscillatorEditScope GetOscillatorEditScope(OscillatorParameter parameter) const {
    return (*model.oscillatorEditScopes)[static_cast<std::size_t>(parameter)];
  }

  void SetOscillatorEditScope(OscillatorParameter parameter, EditorOscillatorEditScope editScope) const {
    (*model.oscillatorEditScopes)[static_cast<std::size_t>(parameter)] = editScope;
  }

  bool IsOscillatorEditable(OscillatorParameter parameter, int oscillatorIndex) const {
    switch (GetOscillatorEditScope(parameter)) {
    case EditorOscillatorEditScope::Even: return !IsOddHarmonic(oscillatorIndex);
    case EditorOscillatorEditScope::Odd:  return IsOddHarmonic(oscillatorIndex);
    case EditorOscillatorEditScope::All:
    default:                              return true;
    }
  }

  void ApplyOscillatorEditScopeToValues(OscillatorParameter parameter, const OscillatorParameterValues& sourceValues,
                                        OscillatorParameterValues& targetValues) const {
    for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
      if (!IsOscillatorEditable(parameter, oscillatorIndex)) {
        targetValues[static_cast<std::size_t>(oscillatorIndex)] = sourceValues[static_cast<std::size_t>(oscillatorIndex)];
      }
    }
  }

  int XRangeMin() const { return *oscillatorView.xRangeMin; }

  int XRangeMax() const { return *oscillatorView.xRangeMax; }

  void SetXRange(int minOscillator, int maxOscillator) const {
    *oscillatorView.xRangeMin = minOscillator;
    *oscillatorView.xRangeMax = maxOscillator;
  }

  bool HasValidSelectedMidiNote() const {
    const int midiNote = SelectedMidiNote();
    return midiNote >= CompoundPatch::kMinMidiNote && midiNote <= CompoundPatch::kMaxMidiNote;
  }

  void ApplyVisibleOscillatorRangeToSliders() const {
    for (auto* control : *oscillatorTabControls.sliderControls) {
      if (control) control->SetVisibleOscillatorRange(XRangeMin(), XRangeMax());
    }
  }

  void SyncXRangeNumberBoxes() const {
    const auto setNumberBoxValueSilently = [](IVNumberBoxControl* control, int value) {
      if (!control || RoundOscillatorRangeValue(control->GetRealValue()) == value) return;

      const auto originalAction = control->GetActionFunction();
      control->SetActionFunction(nullptr);

      WDL_String textValue;
      textValue.SetFormatted(16, "%d", value);
      control->OnTextEntryCompletion(textValue.Get(), 0);
      control->SetActionFunction(originalAction);
      control->SetDirty(false);
    };

    for (std::size_t i = 0; i < oscillatorTabControls.xRangeMinControls->size(); ++i) {
      setNumberBoxValueSilently((*oscillatorTabControls.xRangeMinControls)[i], XRangeMin());
      setNumberBoxValueSilently((*oscillatorTabControls.xRangeMaxControls)[i], XRangeMax());
    }
  }

  void ResetOscillatorRestoreStates() const {
    for (auto* control : *oscillatorTabControls.sliderControls) {
      if (control) control->ClearRestoreState();
    }

    if (eqTab.editorControl && *eqTab.editorControl) (*eqTab.editorControl)->ClearRestoreState();
  }

  bool IsAllKeyNotesEnabled(OscillatorParameter parameter) const { return Patch().IsAllKeyNotesEnabled(parameter); }

  bool IsAllKeyNotesEqEnabled() const { return Patch().IsAllKeyNotesEqEnabled(); }

  template <typename Action> void ForEachTargetKeyNote(OscillatorParameter parameter, int midiNote, Action&& action) const {
    if (IsAllKeyNotesEnabled(parameter)) {
      for (const auto& [keyNoteMidi, _] : Patch().GetKeyNotePatches()) std::forward<Action>(action)(keyNoteMidi);
      return;
    }

    std::forward<Action>(action)(midiNote);
  }

  void SendOscillatorParameterToDSP(IControl* sourceControl, int midiNote, int oscillatorIndex, OscillatorParameter parameter, double value) const {
    if (!sourceControl) return;

    if (auto* delegate = sourceControl->GetDelegate()) {
      editor_messages::SetKeyNoteOscillatorParameterPayload payload;
      payload.midiNote = midiNote;
      payload.oscillatorIndex = oscillatorIndex;
      payload.parameter = static_cast<int>(parameter);
      payload.value = value;
      delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagSetKeyNoteOscillatorParameter, editorTabsTag, sizeof(payload), &payload);
    }
  }

  void SendOscillatorParameterValuesToDSP(IControl* sourceControl, int midiNote, OscillatorParameter parameter,
                                          const std::array<double, SimplePatch::kNumOscillators>& values) const {
    if (!sourceControl) return;

    if (auto* delegate = sourceControl->GetDelegate()) {
      editor_messages::SetKeyNoteOscillatorParameterValuesPayload payload;
      payload.midiNote = midiNote;
      payload.parameter = static_cast<int>(parameter);
      payload.values = values;
      delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagSetKeyNoteOscillatorParameterValues, editorTabsTag, sizeof(payload), &payload);
    }
  }

  void SendAllKeyNotesEnabledToDSP(IControl* sourceControl, OscillatorParameter parameter, bool enabled, int midiNote) const {
    if (!sourceControl) return;

    if (auto* delegate = sourceControl->GetDelegate()) {
      editor_messages::SetAllKeyNotesEnabledPayload payload;
      payload.parameter = static_cast<int>(parameter);
      payload.enabled = enabled ? 1 : 0;
      payload.midiNote = midiNote;
      delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagSetAllKeyNotesEnabled, editorTabsTag, sizeof(payload), &payload);
    }
  }

  void SendEqCurveToDSP(IControl* sourceControl, int midiNote, const EqCurve& curve) const {
    if (!sourceControl) return;

    if (auto* delegate = sourceControl->GetDelegate()) {
      IByteChunk chunk;
      editor_messages::SerializeKeyNoteEqCurvePayload(midiNote, curve, chunk);
      delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagSetKeyNoteEqCurve, editorTabsTag, chunk.Size(), chunk.GetData());
    }
  }

  void SendAllKeyNotesEqEnabledToDSP(IControl* sourceControl, bool enabled) const {
    if (!sourceControl) return;

    if (auto* delegate = sourceControl->GetDelegate()) {
      editor_messages::SetAllKeyNotesEqEnabledPayload payload;
      payload.enabled = enabled ? 1 : 0;
      delegate->SendArbitraryMsgFromUI(editor_messages::kMsgTagSetAllKeyNotesEqEnabled, editorTabsTag, sizeof(payload), &payload);
    }
  }

  void SendKeyNotePatchEditToDSP(IControl* sourceControl, int msgTag, int midiNote) const {
    if (!sourceControl) return;

    if (auto* delegate = sourceControl->GetDelegate()) {
      editor_messages::KeyNotePatchPayload payload;
      payload.midiNote = midiNote;
      delegate->SendArbitraryMsgFromUI(msgTag, editorTabsTag, sizeof(payload), &payload);
    }
  }

  void SetKeyboardKeyNoteHighlight(int midiNote, bool highlighted) const {
    if (keyboardControl && *keyboardControl) (*keyboardControl)->SetHighlightedMidiNote(midiNote, highlighted);
  }

  bool CanAddSelectedKeyNote() const {
    const int midiNote = SelectedMidiNote();
    const bool midiNoteValid = midiNote >= CompoundPatch::kMinMidiNote && midiNote <= CompoundPatch::kMaxMidiNote;
    return IsEditMode() && midiNoteValid && !Patch().HasKeyNotePatch(midiNote);
  }

  bool CanDeleteSelectedKeyNote() const {
    const int midiNote = SelectedMidiNote();
    const bool midiNoteValid = midiNote >= CompoundPatch::kMinMidiNote && midiNote <= CompoundPatch::kMaxMidiNote;
    return IsEditMode() && midiNoteValid && Patch().HasKeyNotePatch(midiNote) && Patch().GetNumKeyNotePatches() > 1;
  }

  void AddSelectedKeyNote(IControl* caller) const {
    if (!caller || !CanAddSelectedKeyNote()) return;

    const int midiNote = SelectedMidiNote();
    Patch().SetKeyNotePatch(midiNote, Patch().GetPatchForMidiNote(midiNote));
    SendKeyNotePatchEditToDSP(caller, editor_messages::kMsgTagAddKeyNotePatch, midiNote);
    SetKeyboardKeyNoteHighlight(midiNote, true);
    RefreshOscillatorTabs();
    RefreshEditorActionButtons();
  }

  void DeleteSelectedKeyNote(IControl* caller) const {
    if (!caller || !CanDeleteSelectedKeyNote()) return;

    const int midiNote = SelectedMidiNote();
    if (!Patch().RemoveKeyNotePatch(midiNote)) return;

    SendKeyNotePatchEditToDSP(caller, editor_messages::kMsgTagRemoveKeyNotePatch, midiNote);
    SetKeyboardKeyNoteHighlight(midiNote, false);
    RefreshOscillatorTabs();
    RefreshEditorActionButtons();
  }

  void RefreshEditorActionButtons() const {
    const bool addEnabled = CanAddSelectedKeyNote();
    const bool deleteEnabled = CanDeleteSelectedKeyNote();

    for (auto* addButton : *oscillatorTabControls.addButtons) SetDisabledState(addButton, !addEnabled);

    for (auto* deleteButton : *oscillatorTabControls.deleteButtons) SetDisabledState(deleteButton, !deleteEnabled);

    SetDisabledState(*eqTab.addButton, !addEnabled);
    SetDisabledState(*eqTab.deleteButton, !deleteEnabled);
  }

  void RefreshOscillatorTabs() const {
    ApplyVisibleOscillatorRangeToSliders();
    SyncXRangeNumberBoxes();

    if (!HasValidSelectedMidiNote()) {
      for (std::size_t i = 0; i < oscillatorTabControls.sliderControls->size(); ++i) {
        if (auto* control = (*oscillatorTabControls.sliderControls)[i]) control->SetEditable(false);

        if (auto* toggle = (*oscillatorTabControls.allKeyNotesToggles)[i]) {
          SetControlValueSilently(toggle, IsAllKeyNotesEnabled(static_cast<OscillatorParameter>(i)) ? 1.0 : 0.0);
          SetDisabledState(toggle, true);
        }

        SetDisabledState((*oscillatorTabControls.restoreButtons)[i], true);
      }

      SetDisabledState(*levelTab.setShapeControl, true);
      SetDisabledState(*levelTab.actionsControl, true);
      SetDisabledState(*breathTab.setShapeControl, true);
      SetDisabledState(*breathTab.actionsControl, true);
      SetDisabledState(*pitchTab.setShapeControl, true);
      SetDisabledState(*pitchTab.actionsControl, true);
      SetDisabledState(*panTab.setShapeControl, true);
      SetDisabledState(*panTab.actionsControl, true);
      for (auto* control : *variationTab.setShapeControls) SetDisabledState(control, true);
      for (auto* control : *variationTab.actionsControls) SetDisabledState(control, true);
      for (auto* control : *attackReleaseTab.setShapeControls) SetDisabledState(control, true);
      for (auto* control : *attackReleaseTab.actionsControls) SetDisabledState(control, true);

      if (eqTab.editorControl && *eqTab.editorControl) {
        (*eqTab.editorControl)->SetCurve(EqCurve{});
        (*eqTab.editorControl)->SetEditable(false);
      }

      SetDisabledState(*eqTab.setShapeControl, true);
      SetDisabledState(*eqTab.actionsControl, true);
      if (eqTab.allKeyNotesToggle && *eqTab.allKeyNotesToggle) {
        (*eqTab.allKeyNotesToggle)->SetValue(IsAllKeyNotesEqEnabled() ? 1.0 : 0.0);
        (*eqTab.allKeyNotesToggle)->SetDirty(false);
        SetDisabledState(*eqTab.allKeyNotesToggle, true);
      }
      SetDisabledState(*eqTab.restoreButton, true);
      return;
    }

    const int midiNote = SelectedMidiNote();
    const SimplePatch* keyNotePatch = Patch().GetKeyNotePatch(midiNote);
    const SimplePatch& selectedPatch = keyNotePatch ? *keyNotePatch : Patch().GetPatchForMidiNote(midiNote);
    const bool editable = keyNotePatch != nullptr;

    SetDisabledState(*levelTab.setShapeControl, !editable);
    SetDisabledState(*levelTab.actionsControl, !editable);
    SetDisabledState(*breathTab.setShapeControl, !editable);
    SetDisabledState(*breathTab.actionsControl, !editable);
    SetDisabledState(*pitchTab.setShapeControl, !editable);
    SetDisabledState(*pitchTab.actionsControl, !editable);
    SetDisabledState(*panTab.setShapeControl, !editable);
    SetDisabledState(*panTab.actionsControl, !editable);
    for (auto* control : *variationTab.setShapeControls) SetDisabledState(control, !editable);
    for (auto* control : *variationTab.actionsControls) SetDisabledState(control, !editable);
    for (auto* control : *attackReleaseTab.setShapeControls) SetDisabledState(control, !editable);
    for (auto* control : *attackReleaseTab.actionsControls) SetDisabledState(control, !editable);
    SetDisabledState(*eqTab.setShapeControl, !editable);
    SetDisabledState(*eqTab.actionsControl, !editable);

    for (const auto& descriptor : GetOscillatorTabDescriptors()) {
      auto* control = (*oscillatorTabControls.sliderControls)[static_cast<std::size_t>(descriptor.parameter)];
      if (!control) continue;

      bool sliderChanged = false;
      for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
        const double value = selectedPatch.GetOscillatorSettings(oscillatorIndex).GetParameter(descriptor.parameter);
        if (AreNearlyEqual(control->GetOscillatorValue(oscillatorIndex), value)) continue;

        control->SetOscillatorValue(oscillatorIndex, value);
        sliderChanged = true;
      }

      if (!control->IsHidden() && !control->HasRestoreStateForMidiNote(midiNote)) control->CaptureRestoreState(midiNote);

      control->SetEditable(editable);
      if (sliderChanged) control->SetDirty(false);

      if (auto* toggle = (*oscillatorTabControls.allKeyNotesToggles)[static_cast<std::size_t>(descriptor.parameter)]) {
        SetControlValueSilently(toggle, IsAllKeyNotesEnabled(descriptor.parameter) ? 1.0 : 0.0);
        SetDisabledState(toggle, !editable);
      }

      auto* restoreButton = (*oscillatorTabControls.restoreButtons)[static_cast<std::size_t>(descriptor.parameter)];
      SetDisabledState(restoreButton, !(editable && control->HasRestoreStateForMidiNote(midiNote)));
    }

    if (eqTab.editorControl && *eqTab.editorControl) {
      const EqCurve* keyNoteEqCurve = Patch().GetKeyNoteEqCurve(midiNote);
      (*eqTab.editorControl)->SetCurve(keyNoteEqCurve ? *keyNoteEqCurve : Patch().GetEqCurveForMidiNote(midiNote));
      if (!(*eqTab.editorControl)->IsHidden() && !(*eqTab.editorControl)->HasRestoreStateForMidiNote(midiNote))
        (*eqTab.editorControl)->CaptureRestoreState(midiNote);
      (*eqTab.editorControl)->SetEditable(editable);
      (*eqTab.editorControl)->SetDirty(false);
    }

    if (eqTab.allKeyNotesToggle && *eqTab.allKeyNotesToggle) {
      SetControlValueSilently(*eqTab.allKeyNotesToggle, IsAllKeyNotesEqEnabled() ? 1.0 : 0.0);
      SetDisabledState(*eqTab.allKeyNotesToggle, !editable);
    }

    SetDisabledState(*eqTab.restoreButton, !((*eqTab.editorControl) && editable && (*eqTab.editorControl)->HasRestoreStateForMidiNote(midiNote)));
  }

  template <typename Action>
  void ApplyOscillatorParameterActionToSelectedKeyNote(OscillatorSliderControl* control, OscillatorParameter parameter, Action&& action,
                                                       bool applyEditScope = true) const {
    const int midiNote = SelectedMidiNote();
    const SimplePatch* keyNotePatch = Patch().GetKeyNotePatch(midiNote);
    if (!keyNotePatch) return;

    SimplePatch updatedPatch = *keyNotePatch;
    if (!std::forward<Action>(action)(updatedPatch)) return;

    const auto originalValues = GetOscillatorParameterValues(*keyNotePatch, parameter);
    OscillatorParameterValues values{};
    for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
      values[static_cast<std::size_t>(oscillatorIndex)] = updatedPatch.GetOscillatorSettings(oscillatorIndex).GetParameter(parameter);
    }
    if (applyEditScope) ApplyOscillatorEditScopeToValues(parameter, originalValues, values);

    if (!Patch().SetKeyNoteOscillatorParameterValues(midiNote, parameter, values)) return;

    SendOscillatorParameterValuesToDSP(control, midiNote, parameter, values);
    RefreshOscillatorTabs();
  }

  template <typename Action> void ApplyEqCurveActionToSelectedKeyNote(EqEditorControl* control, Action&& action) const {
    const int midiNote = SelectedMidiNote();
    const EqCurve* keyNoteEqCurve = Patch().GetKeyNoteEqCurve(midiNote);
    if (!keyNoteEqCurve) return;

    EqCurve updatedCurve = *keyNoteEqCurve;
    if (!std::forward<Action>(action)(updatedCurve)) return;

    if (!Patch().SetKeyNoteEqCurve(midiNote, updatedCurve)) return;

    SendEqCurveToDSP(control, midiNote, updatedCurve);
    RefreshOscillatorTabs();
  }
};

struct XRangeControls {
  IVNumberBoxControl* minControl{};
  IVNumberBoxControl* maxControl{};
};

struct AllKeyNotesControls {
  IVToggleControl* toggleControl{};
  ITextControl* labelControl{};
};

struct KeyNoteActionButtons {
  IVButtonControl* addButton{};
  IVButtonControl* deleteButton{};
};

inline KeyNoteActionButtons CreateKeyNoteActionButtons(const std::shared_ptr<EditorContext>& context, const EditorStyles& styles) {
  auto* addButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Add", styles.restoreButtonStyle, true, false);
  addButton->SetTooltip(help_text::oscillator_tabs::kAddButton);
  addButton->SetAnimationEndActionFunction([context](IControl* caller) { context->AddSelectedKeyNote(caller); });

  auto* deleteButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Del", styles.restoreButtonStyle, true, false);
  deleteButton->SetTooltip(help_text::oscillator_tabs::kDeleteButton);
  deleteButton->SetAnimationEndActionFunction([context](IControl* caller) { context->DeleteSelectedKeyNote(caller); });

  return {addButton, deleteButton};
}

inline AllKeyNotesControls CreateAllKeyNotesControls(const std::shared_ptr<EditorContext>& context, const OscillatorTabDescriptor& descriptor,
                                                     const EditorStyles& styles) {
  auto* toggleControl = new IVToggleControl(
      IRECT(),
      [context, parameter = descriptor.parameter](IControl* caller) {
        auto* toggle = caller ? caller->As<IVToggleControl>() : nullptr;
        if (!toggle || !context->HasValidSelectedMidiNote()) return;

        const int midiNote = context->SelectedMidiNote();
        const SimplePatch* keyNotePatch = context->Patch().GetKeyNotePatch(midiNote);
        if (!keyNotePatch) {
          SetControlValueSilently(toggle, context->IsAllKeyNotesEnabled(parameter) ? 1.0 : 0.0);
          return;
        }

        if (toggle->GetValue() > 0.5) {
          const auto values = GetOscillatorParameterValues(*keyNotePatch, parameter);
          context->Patch().EnableAllKeyNotes(parameter, values);
          context->SendAllKeyNotesEnabledToDSP(toggle, parameter, true, midiNote);
        } else {
          context->Patch().SetAllKeyNotesEnabled(parameter, false);
          context->SendAllKeyNotesEnabledToDSP(toggle, parameter, false, midiNote);
        }

        context->RefreshOscillatorTabs();
      },
      "", styles.utilityToggleStyle, "", "X", context->IsAllKeyNotesEnabled(descriptor.parameter));
  toggleControl->SetTooltip(help_text::oscillator_tabs::kAllNotes);

  auto* labelControl = new ITextControl(IRECT(), "All notes", styles.utilityLabelText, COLOR_TRANSPARENT);
  labelControl->SetIgnoreMouse(false);
  labelControl->SetTooltip(help_text::oscillator_tabs::kAllNotes);
  labelControl->DisablePrompt(true);

  (*context->oscillatorTabControls.allKeyNotesToggles)[static_cast<std::size_t>(descriptor.parameter)] = toggleControl;
  return {toggleControl, labelControl};
}

inline void AttachHarmonicTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                      const OscillatorTabDescriptor& descriptor, const XRangeControls& xRangeControls,
                                      ActionSelectionControl* yTransformControl, ActionSelectionControl* setShapeControl,
                                      ActionSelectionControl* actionsControl, const AllKeyNotesControls& allKeyNotesControls, IVButtonControl* restoreButton,
                                      IVButtonControl* addButton, IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  const char* const actionsTooltip = help_text::oscillator_tabs::GetHarmonicActions(descriptor.parameter);

  page->AddChildControl(CreateUtilityLabelControl("X range:", styles));
  page->AddChildControl(xRangeControls.minControl);
  page->AddChildControl(xRangeControls.maxControl);
  page->AddChildControl(CreateUtilityLabelControl("Y transform:", styles, help_text::oscillator_tabs::kYTransform));
  page->AddChildControl(yTransformControl);
  page->AddChildControl(CreateEditModeLabelControl(styles));
  page->AddChildControl(CreateEditModeControl(context->model.oscillatorEditModes, descriptor, styles));
  page->AddChildControl(CreateEditModeScopeControl(context->model.oscillatorEditScopes, descriptor, styles));
  page->AddChildControl(CreateUtilityLabelControl("Set shape:", styles, help_text::oscillator_tabs::kHarmonicSetShape));
  setShapeControl->SetTooltip(help_text::oscillator_tabs::kHarmonicSetShape);
  page->AddChildControl(setShapeControl);
  page->AddChildControl(CreateUtilityLabelControl("Actions:", styles, actionsTooltip));
  actionsControl->SetTooltip(actionsTooltip);
  page->AddChildControl(actionsControl);
  page->AddChildControl(allKeyNotesControls.toggleControl);
  page->AddChildControl(allKeyNotesControls.labelControl);
  page->AddChildControl(restoreButton);
  page->AddChildControl(addButton);
  page->AddChildControl(deleteButton);
  page->AddChildControl(sliderControl);
}

inline IRECT GetOscillatorSliderBounds(IContainerBase* pTab, const IRECT& r, float leftInset) {
  constexpr float kColumnControlInset = 8.f;
  constexpr float kOuterInset = 2.f;

  const float padding = static_cast<float>(pTab->As<IVTabPage>()->GetPadding());
  return IRECT(r.L + padding + leftInset - kColumnControlInset, r.T + kOuterInset, r.R - kOuterInset, r.B - kOuterInset);
}

inline IRECT GetHarmonicTabLabelBounds(const IRECT& controlBounds, float rowL, float rowR) {
  return IRECT(rowL, controlBounds.T - kHarmonicTabLabelGap - kHarmonicTabLabelHeight, rowR, controlBounds.T - kHarmonicTabLabelGap);
}

inline HarmonicTabLayout GetHarmonicTabLayout(IContainerBase* pTab, const IRECT& r) {
  auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
  auto leftColumnBounds = innerBounds.GetFromLeft(kHarmonicTabLeftInset);

  const float rowL = leftColumnBounds.L + kHarmonicTabSideInset;
  const float rowR = leftColumnBounds.R - kHarmonicTabSideInset;
  const float rowMid = (rowL + rowR) * 0.5f;

  HarmonicTabLayout layout;
  layout.sliderBounds = GetOscillatorSliderBounds(pTab, r, kHarmonicTabLeftInset);

  const float buttonRowBottom = leftColumnBounds.B - kHarmonicTabBottomPad;
  const float buttonRowTop = buttonRowBottom - kEditorControlHeight;
  layout.addButtonBounds = IRECT(rowL, buttonRowTop, rowMid - kTabButtonHalfGap, buttonRowBottom);
  layout.deleteButtonBounds = IRECT(rowMid + kTabButtonHalfGap, buttonRowTop, rowR, buttonRowBottom);

  const float restoreTop = layout.addButtonBounds.T - kHarmonicTabControlGap - kEditorControlHeight;
  layout.restoreButtonBounds = IRECT(rowL, restoreTop, rowR, restoreTop + kEditorControlHeight);

  const float allKeyNotesTop = layout.restoreButtonBounds.T - kHarmonicTabControlGap - kEditorControlHeight;
  layout.allKeyNotesToggleBounds = IRECT(rowL, allKeyNotesTop, rowL + kEditorControlHeight, allKeyNotesTop + kEditorControlHeight);
  layout.allKeyNotesLabelBounds =
      IRECT(layout.allKeyNotesToggleBounds.R + kHarmonicTabToggleLabelGap, layout.allKeyNotesToggleBounds.T, rowR, layout.allKeyNotesToggleBounds.B);

  const float actionsTop = layout.allKeyNotesToggleBounds.T - kHarmonicTabControlGap - kEditorControlHeight;
  layout.actionsBounds = IRECT(rowL, actionsTop, rowR, actionsTop + kEditorControlHeight);
  layout.actionsLabelBounds = GetHarmonicTabLabelBounds(layout.actionsBounds, rowL, rowR);

  const float setShapeTop = layout.actionsLabelBounds.T - kHarmonicTabControlGap - kEditorControlHeight;
  layout.setShapeBounds = IRECT(rowL, setShapeTop, rowR, setShapeTop + kEditorControlHeight);
  layout.setShapeLabelBounds = GetHarmonicTabLabelBounds(layout.setShapeBounds, rowL, rowR);

  const float scopeTop = layout.setShapeLabelBounds.T - kHarmonicTabScopeSectionGap - kEditorControlHeight;
  layout.scopeBounds = IRECT(rowL, scopeTop, leftColumnBounds.R - kHarmonicTabScopeRightInset, scopeTop + kEditorControlHeight);

  const float editModeTop = layout.scopeBounds.T - kHarmonicTabScopeGap - kEditorControlHeight;
  layout.editModeBounds = IRECT(rowL, editModeTop, rowR, editModeTop + kEditorControlHeight);
  layout.editModeLabelBounds = GetHarmonicTabLabelBounds(layout.editModeBounds, rowL, rowR);

  const float yTransformTop = layout.editModeLabelBounds.T - kHarmonicTabControlGap - kEditorControlHeight;
  layout.yTransformBounds = IRECT(rowL, yTransformTop, rowR, yTransformTop + kEditorControlHeight);
  layout.yTransformLabelBounds = GetHarmonicTabLabelBounds(layout.yTransformBounds, rowL, rowR);

  const float xRangeTop = layout.yTransformLabelBounds.T - kHarmonicTabControlGap - kEditorControlHeight;
  layout.xRangeMinBounds = IRECT(rowL, xRangeTop, rowMid - (kHarmonicTabXRangeHalfGap * 0.5f), xRangeTop + kEditorControlHeight);
  layout.xRangeMaxBounds = IRECT(rowMid + (kHarmonicTabXRangeHalfGap * 0.5f), layout.xRangeMinBounds.T, rowR, layout.xRangeMinBounds.B);
  layout.xRangeLabelBounds = GetHarmonicTabLabelBounds(layout.xRangeMinBounds, rowL, rowR);

  return layout;
}

inline void ResizeHarmonicOscillatorTabPage(IContainerBase* pTab, const IRECT& r) {
  if (pTab->NChildren() < kHarmonicTabChildCount) return;

  const auto layout = GetHarmonicTabLayout(pTab, r);
  const std::array<IRECT, kHarmonicTabChildCount> childBounds{
      {layout.xRangeLabelBounds, layout.xRangeMinBounds, layout.xRangeMaxBounds, layout.yTransformLabelBounds, layout.yTransformBounds,
       layout.editModeLabelBounds, layout.editModeBounds, layout.scopeBounds, layout.setShapeLabelBounds, layout.setShapeBounds, layout.actionsLabelBounds,
       layout.actionsBounds, layout.allKeyNotesToggleBounds, layout.allKeyNotesLabelBounds, layout.restoreButtonBounds, layout.addButtonBounds,
       layout.deleteButtonBounds, layout.sliderBounds}};

  for (std::size_t i = 0; i < childBounds.size(); ++i) pTab->GetChild(static_cast<int>(i))->SetTargetAndDrawRECTs(childBounds[i]);
}

inline void ResizeDefaultOscillatorTabPage(IContainerBase* pTab, const IRECT& r) {
  if (pTab->NChildren() < 12) return;

  constexpr float kLeftInset = 104.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = kEditorControlHeight;
  constexpr float kButtonHeight = kEditorControlHeight;
  constexpr float kBottomPad = 8.f;
  constexpr float kTightGap = 0.f;
  constexpr float kGap = 10.f;
  constexpr float kHalfGap = 6.f;
  constexpr float kToggleLabelGap = 8.f;
  constexpr float kScopeGap = 0.f;

  auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
  auto leftColumnBounds = innerBounds.GetFromLeft(kLeftInset);
  const float rowL = leftColumnBounds.L + 8.f;
  const float rowR = leftColumnBounds.R - 8.f;
  const float rowMid = (rowL + rowR) * 0.5f;
  const float scopeRowL = rowL;
  const float scopeRowR = leftColumnBounds.R - 4.f;
  const float buttonRowBottom = leftColumnBounds.B - kBottomPad;
  const float buttonRowTop = buttonRowBottom - kButtonHeight;
  auto addButtonBounds = IRECT(rowL, buttonRowTop, rowMid - kTabButtonHalfGap, buttonRowBottom);
  auto deleteButtonBounds = IRECT(rowMid + kTabButtonHalfGap, buttonRowTop, rowR, buttonRowBottom);
  const float restoreTop = addButtonBounds.T - kGap - kButtonHeight;
  auto restoreButtonBounds = IRECT(rowL, restoreTop, rowR, restoreTop + kButtonHeight);
  const float allKeyNotesTop = restoreButtonBounds.T - kGap - kControlHeight;
  auto allKeyNotesToggleBounds = IRECT(rowL, allKeyNotesTop, rowL + kControlHeight, allKeyNotesTop + kControlHeight);
  auto allKeyNotesLabelBounds = IRECT(allKeyNotesToggleBounds.R + kToggleLabelGap, allKeyNotesToggleBounds.T, rowR, allKeyNotesToggleBounds.B);
  const float scopeTop = allKeyNotesToggleBounds.T - kGap - kControlHeight;
  auto scopeBounds = IRECT(scopeRowL, scopeTop, scopeRowR, scopeTop + kControlHeight);
  const float editModeTop = scopeBounds.T - kScopeGap - kControlHeight;
  auto editModeBounds = IRECT(rowL, editModeTop, rowR, editModeTop + kControlHeight);
  auto editModeLabelBounds = IRECT(rowL, editModeBounds.T - kTightGap - kLabelHeight, rowR, editModeBounds.T - kTightGap);
  const float xRangeTop = editModeLabelBounds.T - kGap - kControlHeight;
  auto xRangeMinBounds = IRECT(rowL, xRangeTop, rowMid - kHalfGap * 0.5f, xRangeTop + kControlHeight);
  auto xRangeMaxBounds = IRECT(rowMid + kHalfGap * 0.5f, xRangeMinBounds.T, rowR, xRangeMinBounds.B);
  auto xRangeLabelBounds = IRECT(rowL, xRangeMinBounds.T - kTightGap - kLabelHeight, rowR, xRangeMinBounds.T - kTightGap);
  const auto sliderBounds = GetOscillatorSliderBounds(pTab, r, kLeftInset);

  pTab->GetChild(0)->SetTargetAndDrawRECTs(xRangeLabelBounds);
  pTab->GetChild(1)->SetTargetAndDrawRECTs(xRangeMinBounds);
  pTab->GetChild(2)->SetTargetAndDrawRECTs(xRangeMaxBounds);
  pTab->GetChild(3)->SetTargetAndDrawRECTs(editModeLabelBounds);
  pTab->GetChild(4)->SetTargetAndDrawRECTs(editModeBounds);
  pTab->GetChild(5)->SetTargetAndDrawRECTs(scopeBounds);
  pTab->GetChild(6)->SetTargetAndDrawRECTs(allKeyNotesToggleBounds);
  pTab->GetChild(7)->SetTargetAndDrawRECTs(allKeyNotesLabelBounds);
  pTab->GetChild(8)->SetTargetAndDrawRECTs(restoreButtonBounds);
  pTab->GetChild(9)->SetTargetAndDrawRECTs(addButtonBounds);
  pTab->GetChild(10)->SetTargetAndDrawRECTs(deleteButtonBounds);
  pTab->GetChild(11)->SetTargetAndDrawRECTs(sliderBounds);
}

inline void RestoreOscillatorTabValues(const std::shared_ptr<EditorContext>& context, IControl* caller, const OscillatorTabDescriptor& descriptor) {
  if (!caller || !context->HasValidSelectedMidiNote()) return;

  const int midiNote = context->SelectedMidiNote();
  const SimplePatch* keyNotePatch = context->Patch().GetKeyNotePatch(midiNote);
  if (!keyNotePatch) return;

  auto* control = (*context->oscillatorTabControls.sliderControls)[static_cast<std::size_t>(descriptor.parameter)];
  if (!control || !control->HasRestoreStateForMidiNote(midiNote)) return;

  const auto& restoreState = control->GetRestoreState();
  OscillatorParameterValues values{};
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    values[static_cast<std::size_t>(oscillatorIndex)] =
        std::clamp(restoreState[static_cast<std::size_t>(oscillatorIndex)], descriptor.range.min, descriptor.range.max);
  }

  if (!context->Patch().SetKeyNoteOscillatorParameterValues(midiNote, descriptor.parameter, values)) return;

  context->SendOscillatorParameterValuesToDSP(caller, midiNote, descriptor.parameter, values);

  context->RefreshOscillatorTabs();
}

inline OscillatorSliderControl* CreateOscillatorSliderControl(const std::shared_ptr<EditorContext>& context, const OscillatorTabDescriptor& descriptor,
                                                              const EditorStyles& styles) {
  auto* control = new OscillatorSliderControl(IRECT(), "", styles.sliderStyle, EDirection::Vertical);
  OscillatorSliderControl::Config config;
  config.range = descriptor.range;
  if (descriptor.parameter == OscillatorParameter::level) config.transform = GetSliderValueTransform(*context->levelTab.levelTransform);
  else if (descriptor.parameter == OscillatorParameter::breath_power)
    config.transform = GetSliderValueTransform(*context->breathTab.breathTransform);
  else if (descriptor.parameter == OscillatorParameter::pitch)
    config.transform = GetSliderValueTransform(*context->pitchTab.pitchTransform);
  else if (descriptor.parameter == OscillatorParameter::pan)
    config.transform = GetSliderValueTransform(*context->panTab.panTransform);
  else if (IsVariationParameter(descriptor.parameter))
    config.transform = GetSliderValueTransform((*context->variationTab.transforms)[GetVariationTabIndex(descriptor.parameter)]);
  else if (descriptor.parameter == OscillatorParameter::attack)
    config.transform = GetSliderValueTransform(*context->attackReleaseTab.attackTransform);
  else if (descriptor.parameter == OscillatorParameter::release)
    config.transform = GetSliderValueTransform(*context->attackReleaseTab.releaseTransform);
  control->SetConfig(config);
  control->SetOscillatorEditModeFunc([context, parameter = descriptor.parameter]() { return context->GetOscillatorEditMode(parameter); });
  control->SetOscillatorEditableFunc(
      [context, parameter = descriptor.parameter](int oscillatorIndex) { return context->IsOscillatorEditable(parameter, oscillatorIndex); });
  control->SetVisibleOscillatorRange(context->XRangeMin(), context->XRangeMax());
  control->SetOnOscillatorValueChanged([context, control, descriptor](int oscillatorIndex, double value) {
    if (oscillatorIndex < 0 || oscillatorIndex >= SimplePatch::kNumOscillators) return;

    const int midiNote = context->SelectedMidiNote();
    if (!context->IsOscillatorEditable(descriptor.parameter, oscillatorIndex)) return;

    const double clampedValue = std::clamp(value, descriptor.range.min, descriptor.range.max);
    const bool updated = context->Patch().SetKeyNoteOscillatorParameter(midiNote, oscillatorIndex, descriptor.parameter, clampedValue);
    if (!updated) return;

    context->SendOscillatorParameterToDSP(control, midiNote, oscillatorIndex, descriptor.parameter, clampedValue);
  });
  return control;
}

inline XRangeControls CreateXRangeControls(const std::shared_ptr<EditorContext>& context, const OscillatorTabDescriptor& descriptor,
                                           const EditorStyles& styles) {
  auto* minControl = new IVNumberBoxControl(IRECT(), kNoParameter, nullptr, "", styles.utilityNumberBoxStyle, false, static_cast<double>(context->XRangeMin()),
                                            1.0, static_cast<double>(SimplePatch::kNumOscillators), "%0.0f", false);
  auto* maxControl = new IVNumberBoxControl(IRECT(), kNoParameter, nullptr, "", styles.utilityNumberBoxStyle, false, static_cast<double>(context->XRangeMax()),
                                            1.0, static_cast<double>(SimplePatch::kNumOscillators), "%0.0f", false);
  minControl->SetDrawTriangle(false);
  maxControl->SetDrawTriangle(false);
  minControl->SetTooltip(help_text::oscillator_tabs::kXRangeMin);
  maxControl->SetTooltip(help_text::oscillator_tabs::kXRangeMax);

  auto rangeGuard = std::make_shared<bool>(false);
  const auto clampEditedControl = [rangeGuard, minControl, maxControl](IVNumberBoxControl* editedControl) {
    if (*rangeGuard) return;

    const double minValue = minControl->GetRealValue();
    const double maxValue = maxControl->GetRealValue();
    if (minValue <= maxValue) return;

    *rangeGuard = true;
    WDL_String textValue;
    textValue.SetFormatted(16, "%0.0f", editedControl == minControl ? maxValue : minValue);
    editedControl->OnTextEntryCompletion(textValue.Get(), 0);
    *rangeGuard = false;
  };
  const auto updateVisibleRange = [context, minControl, maxControl]() {
    context->SetXRange(RoundOscillatorRangeValue(minControl->GetRealValue()), RoundOscillatorRangeValue(maxControl->GetRealValue()));
    context->ApplyVisibleOscillatorRangeToSliders();
    context->SyncXRangeNumberBoxes();
  };

  minControl->SetActionFunction([clampEditedControl, updateVisibleRange, minControl](IControl*) {
    clampEditedControl(minControl);
    updateVisibleRange();
  });
  maxControl->SetActionFunction([clampEditedControl, updateVisibleRange, maxControl](IControl*) {
    clampEditedControl(maxControl);
    updateVisibleRange();
  });

  const auto parameterIndex = static_cast<std::size_t>(descriptor.parameter);
  (*context->oscillatorTabControls.xRangeMinControls)[parameterIndex] = minControl;
  (*context->oscillatorTabControls.xRangeMaxControls)[parameterIndex] = maxControl;
  context->SyncXRangeNumberBoxes();
  return {minControl, maxControl};
}
} // namespace editor
} // namespace plugin_ui

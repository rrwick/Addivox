#pragma once

#include "../../settings/oscillator.h"
#include "../colour.h"
#include "../knob.h"
#include "IControls.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

namespace editor {
using MacroOscillatorParameterValues = CompoundPatch::OscillatorParameterValues;

// Macro knobs go in the column the hand-edit controls vacate, at hand-picked positions rather than on a grid.
// Four knobs leave most of the column empty, and a staggered pair of columns reads better than a block, which is
// not something a row-and-gap rule expresses well. The label is drawn across the full knob width and centred, so
// a name wider than the knob -- "Odd/Even" -- simply spills into the empty corner beside it.
inline constexpr float        kMacroKnobSize = 48.f;
inline constexpr float kMacroKnobLabelHeight = 13.f;
inline constexpr float    kMacroKnobLabelGap =  1.f;

// Where each knob sits: pixels right and down from the top-left of the macro area, one entry per knob in the
// order the tab lists them. These are the numbers to tweak, and a tab with more knobs than this adds lines.
struct MacroKnobPosition {
  float x{0.f};
  float y{0.f};
};

inline constexpr std::array<MacroKnobPosition, 4> kMacroKnobPositions{{
    { 0.f,  10.f},  // Width
    {48.f,  58.f},  // Shape
    { 0.f, 106.f},  // Fund
    {48.f, 154.f},  // Odd/Even
}};

inline IText GetMacroKnobLabelText() { return {12.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Center}; }

// One row of a tab's knob table. Adding, removing or renaming a macro knob should be this and nothing else.
struct MacroKnobDescriptor {
  const char* label{""};
  const char* tooltip{""};
  double defaultValue{0.0}; // Normalised 0..1, and where a double-tap returns to.
  bool bipolar{false};      // Draws the value arc out from the centre rather than from the left.
};

// What a tab with macros can do, registered by that tab when it attaches its knobs. Tabs with no generator
// yet leave these empty, which is what an empty Macro mode means.
struct MacroTabFunctions {
  std::function<void(const MacroOscillatorParameterValues& values)> fitKnobsToValues;
  std::function<MacroOscillatorParameterValues()> generateValues;

  bool IsValid() const { return fitKnobsToValues && generateValues; }
};

// The array the knobs were last fitted to, or last wrote. Anything else moving the tab's array -- a key note
// change, Restore, a hand edit made before switching modes -- shows up as a divergence from this, and that is
// what triggers a refit. Comparing beats invalidating, which would need every write site to remember to.
struct MacroFitState {
  bool valid{false};
  MacroOscillatorParameterValues values{};
};

inline layout::LabelledKnob* CreateMacroKnobControl(const MacroKnobDescriptor& descriptor, std::function<void()> onValueChanged) {
  layout::UnboundKnobSpec spec;
  spec.defaultValue = descriptor.defaultValue;
  spec.bipolar = descriptor.bipolar;
  spec.onValueChanged = [onValueChanged = std::move(onValueChanged)](double) {
    if (onValueChanged) onValueChanged();
  };

  auto* control = new layout::LabelledKnob(IRECT(), descriptor.label, std::move(spec), kMacroKnobLabelGap);
  control->SetLabelStyle(GetMacroKnobLabelText(), kMacroKnobLabelHeight);
  control->SetMaxKnobSize(kMacroKnobSize);
  control->SetTooltip(descriptor.tooltip);
  return control;
}

inline std::vector<IRECT> GetMacroKnobBounds(const IRECT& macroAreaBounds, int numKnobs) {
  const auto placedKnobs = std::min<std::size_t>(static_cast<std::size_t>(std::max(numKnobs, 0)), kMacroKnobPositions.size());

  std::vector<IRECT> knobBounds;
  knobBounds.reserve(placedKnobs);
  const float boxHeight = kMacroKnobSize + kMacroKnobLabelGap + kMacroKnobLabelHeight;

  for (std::size_t knobIndex = 0; knobIndex < placedKnobs; ++knobIndex) {
    const auto& position = kMacroKnobPositions[knobIndex];
    knobBounds.push_back(IRECT::MakeXYWH(macroAreaBounds.L + position.x, macroAreaBounds.T + position.y, kMacroKnobSize, boxHeight));
  }

  return knobBounds;
}
} // namespace editor
} // namespace plugin_ui

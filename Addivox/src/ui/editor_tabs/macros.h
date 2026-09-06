#pragma once

#include "../../settings/oscillator.h"
#include "../colour.h"
#include "../knob.h"
#include "IControls.h"

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

// Macro knobs go in the column the hand-edit controls vacate: two per row, sized so that three rows of two
// fit the space with room to spare. The knob is capped narrower than its cell so a label can be wider than
// the knob it names, which is what makes "Odd/Even" fit a 48px column.
inline constexpr int      kMacroKnobColumns =  2;
inline constexpr float  kMacroKnobRowHeight = 58.f;
inline constexpr float     kMacroKnobRowGap = 12.f;
inline constexpr float        kMacroKnobMax = 42.f;
inline constexpr float kMacroKnobLabelHeight = 11.f;
inline constexpr float   kMacroKnobLabelGap =  1.f;

inline IText GetMacroKnobLabelText() { return {11.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Center}; }

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
  control->SetMaxKnobSize(kMacroKnobMax);
  control->SetTooltip(descriptor.tooltip);
  return control;
}

inline std::vector<IRECT> GetMacroKnobBounds(const IRECT& macroAreaBounds, int numKnobs) {
  std::vector<IRECT> knobBounds;
  if (numKnobs <= 0) return knobBounds;

  knobBounds.reserve(static_cast<std::size_t>(numKnobs));
  const float cellWidth = macroAreaBounds.W() / static_cast<float>(kMacroKnobColumns);

  for (int knobIndex = 0; knobIndex < numKnobs; ++knobIndex) {
    const float left = macroAreaBounds.L + (cellWidth * static_cast<float>(knobIndex % kMacroKnobColumns));
    const float top = macroAreaBounds.T + (static_cast<float>(knobIndex / kMacroKnobColumns) * (kMacroKnobRowHeight + kMacroKnobRowGap));
    knobBounds.push_back(IRECT::MakeXYWH(left, top, cellWidth, kMacroKnobRowHeight));
  }

  return knobBounds;
}
} // namespace editor
} // namespace plugin_ui

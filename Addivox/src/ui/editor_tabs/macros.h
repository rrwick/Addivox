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

// Knobs are staggered within the narrow control column. Label boxes overlap, so attach order matters:
// each overlapping box must precede the knob it covers, or it will swallow that knob's clicks.
inline constexpr float        kMacroKnobSize = 46.f;
inline constexpr float    kMacroKnobBoxWidth = 60.f;
inline constexpr float kMacroKnobLabelHeight = 13.f;
inline constexpr float    kMacroKnobLabelGap =  1.f;
inline constexpr float   kMacroKnobEdgeSlack =  1.f;

static_assert(kMacroKnobBoxWidth >= kMacroKnobSize, "The macro knob box must be at least as wide as the knob it holds");

// Knob top-left offsets within the macro area, in descriptor order.
struct MacroKnobPosition {
  float x{0.f};
  float y{0.f};
};

inline constexpr std::array<MacroKnobPosition, 4> kMacroKnobPositions{{
    { 0.f,  10.f},
    {48.f,  58.f},
    { 0.f, 106.f},
    {48.f, 154.f},
}};

inline IText GetMacroKnobLabelText() { return {13.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Center}; }

struct MacroKnobDescriptor {
  const char* label{""};
  const char* tooltip{""};
  double defaultValue{0.0}; // Normalised 0..1, and where a double-tap returns to.
  bool bipolar{false};      // Draws the value arc out from the centre rather than from the left.
};

// Registered by each implemented tab. Position storage and recall are shared; only fitting and generation vary.
struct MacroTabFunctions {
  int version{0}; // Bump only when changing a released macro definition.
  std::vector<layout::LabelledKnob*> knobs;
  std::function<void(const MacroOscillatorParameterValues& values)> fitKnobsToValues;
  std::function<MacroOscillatorParameterValues()> generateValues;

  bool IsValid() const { return !knobs.empty() && fitKnobsToValues && generateValues; }
  bool CanRecall(const MacroSettings& settings) const {
    return IsValid() && settings.IsValid() && settings.version == version && settings.positions.size() == knobs.size();
  }
  MacroSettings ReadSettings() const {
    MacroSettings settings{version, {}};
    settings.positions.reserve(knobs.size());
    for (auto* knob : knobs) settings.positions.push_back(knob->GetNormalizedValue());
    return settings;
  }
  void Recall(const MacroSettings& settings) const {
    for (std::size_t i = 0; i < knobs.size(); ++i) knobs[i]->SetNormalizedValueSilently(settings.positions[i]);
  }
};

struct MacroTabState {
  bool macrosMode{false};
  int midiNote{-1};
  MacroSettings savedSettings;
  MacroSettings knobSettings; // Last displayed positions; suppresses callbacks from clicks without movement.
  MacroSettings restoreSettings;
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

// Positions refer to the knob itself. Grow the box for its label and inset the knob from its parent's
// edges: IGraphics clamps dirty rectangles inside the parent, otherwise leaving a stale edge pixel.
inline IRECT GetMacroKnobBounds(const IRECT& macroAreaBounds, std::size_t knobIndex) {
  const auto& position = kMacroKnobPositions[knobIndex];
  const float boxHeight = kMacroKnobSize + kMacroKnobLabelGap + kMacroKnobLabelHeight + kMacroKnobEdgeSlack;
  const float knobInset = (kMacroKnobBoxWidth - kMacroKnobSize) * 0.5f;
  return IRECT::MakeXYWH(macroAreaBounds.L + position.x - knobInset, macroAreaBounds.T + position.y, kMacroKnobBoxWidth, boxHeight);
}

} // namespace editor
} // namespace plugin_ui

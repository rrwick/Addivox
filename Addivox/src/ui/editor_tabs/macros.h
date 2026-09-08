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
// not something a row-and-gap rule expresses well.
//
// The box is much wider than the knob because it has to hold the label, and the longest label is a good deal
// wider than the knob it names: "Odd/Even" is 57px of 13px Roboto-Black against a 46px knob. Every box
// therefore reaches 7px past its own knob on each side, which puts the left column's boxes into the column's
// side inset and the right column's 5px onto the slider -- harmlessly, since the slider keeps 15px of empty
// margin before its first bar.
//
// So the boxes overlap things they do not own, and what keeps that harmless is attach order: where two target
// rects overlap, the control attached later takes the mouse. Every box that covers a knob covers one attached
// after it -- Width's box reaches into Shape's knob, Shape's into Fund's -- so the knob always wins. A stagger
// below kMacroKnobSize would reverse that, dropping each box into the band of the knob above it in the other
// column, which was attached earlier, and the box would start swallowing that knob's clicks. The knobs are the
// only children here that take the mouse at all; readouts set SetIgnoreMouse.
inline constexpr float        kMacroKnobSize = 46.f;  // Matches the main UI's knobs, which solve to 46 in a 50x60 box.
inline constexpr float    kMacroKnobBoxWidth = 60.f;  // Holds "Odd/Even" with ~1.7px spare either side; see GetMacroKnobBounds.
inline constexpr float kMacroKnobLabelHeight = 13.f;
inline constexpr float    kMacroKnobLabelGap =  1.f;
inline constexpr float   kMacroKnobEdgeSlack =  1.f;  // The gap below the label; see GetMacroKnobBounds.

// A box narrower than its knob would silently shrink the knob rather than overflow, since LabelledKnob fits the
// knob to whichever of the box's dimensions is smallest.
static_assert(kMacroKnobBoxWidth >= kMacroKnobSize, "The macro knob box must be at least as wide as the knob it holds");

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

// 13px Roboto-Black, matching the tab's own controls -- theme::EditorStyles::utilityLabelText ("All notes") and
// restoreButtonStyle ("Restore") are the same face at the same size.
inline IText GetMacroKnobLabelText() { return {13.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Center}; }

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

// The position is the knob's own top-left; the box is grown around it, out to kMacroKnobBoxWidth across and
// kMacroKnobEdgeSlack below the label. Neither margin is spare room. A child control that reaches its parent's
// edge has its repaint region pulled back inside itself: IGraphics::IsDirty clanks a dirty rect into the
// parent's bounds, and IRECT::Clank treats touching as overflowing -- it returns rhs.R - 1, not rhs.R. A knob
// that exactly filled its box therefore never repainted its own right-hand column, and left a sliver of
// whatever was underneath until a neighbouring control dirtied a wider region.
//
// So the knob is inset horizontally by the box's extra width and the label is held off the bottom by the slack.
// The label is the one child still flush with its box, left and right, because it is laid out across the full
// width -- that costs it a pixel of repaint on the right, which the ~1.7px the text has spare inside a 60px box
// absorbs. The knob keeps its size through SetMaxKnobSize; the box only grows around it.
inline std::vector<IRECT> GetMacroKnobBounds(const IRECT& macroAreaBounds, int numKnobs) {
  const auto placedKnobs = std::min<std::size_t>(static_cast<std::size_t>(std::max(numKnobs, 0)), kMacroKnobPositions.size());

  std::vector<IRECT> knobBounds;
  knobBounds.reserve(placedKnobs);
  const float boxHeight = kMacroKnobSize + kMacroKnobLabelGap + kMacroKnobLabelHeight + kMacroKnobEdgeSlack;
  const float knobInset = (kMacroKnobBoxWidth - kMacroKnobSize) * 0.5f;

  for (std::size_t knobIndex = 0; knobIndex < placedKnobs; ++knobIndex) {
    const auto& position = kMacroKnobPositions[knobIndex];
    knobBounds.push_back(
        IRECT::MakeXYWH(macroAreaBounds.L + position.x - knobInset, macroAreaBounds.T + position.y, kMacroKnobBoxWidth, boxHeight));
  }

  return knobBounds;
}
} // namespace editor
} // namespace plugin_ui

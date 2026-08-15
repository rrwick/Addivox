#pragma once

#include "IControls.h"

#include <cstring>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

namespace layout {
struct PitchBendRangeMenuItem {
  const char* label;
  int semitones;
};

// The fixed choices offered by both the settings menu and the pitch bend wheel's right-click menu.
inline constexpr PitchBendRangeMenuItem kPitchBendRangeMenuItems[] = {{"Off", 0},     {"1 semitone", 1}, {"2 semitones", 2}, {"Fifth", 7},
                                                                      {"Octave", 12}, {"2 octaves", 24}, {"4 octaves", 48}};

/** Fills a menu with the pitch bend range choices, checking the one matching currentSemitones (none if it is not a listed value). */
inline void PopulatePitchBendRangeMenu(IPopupMenu& menu, int currentSemitones) {
  menu.Clear();

  for (const PitchBendRangeMenuItem& item : kPitchBendRangeMenuItems)
    menu.AddItem(item.label, -1, (item.semitones == currentSemitones) ? IPopupMenu::Item::kChecked : 0);
}

/** Resolves a menu's chosen item to its semitone value, returning false if nothing valid was chosen. */
inline bool TryGetPitchBendRangeFromMenu(IPopupMenu* menu, int& semitones) {
  const IPopupMenu::Item* chosenItem = menu ? menu->GetChosenItem() : nullptr;
  const char* label = chosenItem ? chosenItem->GetText() : nullptr;
  if (!label) return false;

  for (const PitchBendRangeMenuItem& item : kPitchBendRangeMenuItems) {
    if (std::strcmp(label, item.label) == 0) {
      semitones = item.semitones;
      return true;
    }
  }

  return false;
}
} // namespace layout
} // namespace plugin_ui

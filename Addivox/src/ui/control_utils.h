#pragma once

#include "IControls.h"

#include <utility>

namespace plugin_ui {
template <typename TControl> inline TControl* MakePassiveControl(TControl* control) {
  control->SetIgnoreMouse(true);
  control->DisablePrompt(true);
  return control;
}

inline void SetTooltipIfPresent(iplug::igraphics::IControl* control, const char* tooltip) {
  if (control && tooltip && tooltip[0] != '\0') control->SetTooltip(tooltip);
}

template <typename Callback> inline iplug::igraphics::IActionFunction MakeImmediateButtonAction(Callback&& callback) {
  return [cb = std::forward<Callback>(callback)](iplug::igraphics::IControl* caller) mutable {
    if (caller) {
      caller->SetValue(0.);
      caller->SetDirty(false);
    }
    cb(caller);
  };
}
} // namespace plugin_ui

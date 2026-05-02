#pragma once

namespace plugin_ui {
template <typename TControl> inline TControl* MakePassiveControl(TControl* control) {
  control->SetIgnoreMouse(true);
  control->DisablePrompt(true);
  return control;
}
} // namespace plugin_ui

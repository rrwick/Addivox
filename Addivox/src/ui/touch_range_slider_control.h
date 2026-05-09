#pragma once

#include "IControls.h"

#include <cmath>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

namespace layout {
class TouchRangeSliderControl final : public IVRangeSliderControl {
public:
  using IVRangeSliderControl::IVRangeSliderControl;

  void OnAttached() override {
    IVRangeSliderControl::OnAttached();
    if (mDoubleTapGestureAttached || !GetUI()) return;

    AttachGestureRecognizer(EGestureType::DoubleTap, [this](IControl*, const IGestureInfo& info) { ResetNearestHandleToDefault(info.x, info.y); });
    mDoubleTapGestureAttached = true;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override {
    mMouseOverHandle = FindNearestHandle(x, y);
    IVRangeSliderControl::OnMouseDown(x, y, mod);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override {
    ResetNearestHandleToDefault(x, y);
  }

private:
  void ResetNearestHandleToDefault(float x, float y) {
    const int handleIndex = FindNearestHandle(x, y);
    if (handleIndex == -1) return;

    SetValueToDefault(handleIndex);
  }

  int FindNearestHandle(float x, float y) {
    if (!mRECT.Contains(x, y)) return -1;

    for (int handleIndex = 0; handleIndex < NVals(); ++handleIndex) {
      if (GetHandleBounds(handleIndex).Contains(x, y)) return handleIndex;
    }

    int nearestHandle = -1;
    float nearestAxisDistance = 0.f;
    for (int handleIndex = 0; handleIndex < NVals(); ++handleIndex) {
      const IRECT handleBounds = GetHandleBounds(handleIndex);
      const float handlePosition = (mDirection == EDirection::Horizontal) ? handleBounds.MW() : handleBounds.MH();
      const float touchPosition = (mDirection == EDirection::Horizontal) ? x : y;
      const float axisDistance = std::fabs(touchPosition - handlePosition);
      if (nearestHandle == -1 || axisDistance < nearestAxisDistance) {
        nearestHandle = handleIndex;
        nearestAxisDistance = axisDistance;
      }
    }

    return nearestHandle;
  }

  bool mDoubleTapGestureAttached = false;
};
} // namespace layout
} // namespace plugin_ui

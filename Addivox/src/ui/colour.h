#pragma once

#include "IGraphicsStructs.h"

#include <array>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace colour
{
namespace ui
{
// Main window background behind all controls.
static const IColor kPanelBackground{255, 27, 42, 56};

// Base fill for vector controls (knob body, meter track background).
static const IColor kControlBody{255, 20, 22, 28};

// Outline/frame color for vector controls.
static const IColor kControlFrame{255, 88, 96, 110};

// Primary accent used for active/pressed states and indicator tracks.
static const IColor kAccentPrimary{255, 118, 168, 230};

// Secondary accent used as an alternate track/LED tone.
static const IColor kAccentSecondary{255, 90, 214, 205};

// Mouse-over / hover highlight overlay.
static const IColor kControlHighlight{110, 140, 180, 235};

// Soft shadow tone for vector controls.
static const IColor kControlShadow{90, 0, 0, 0};

// Label text color for control captions.
static const IColor kLabelText{255, 188, 188, 188};

// Value readout text color for control values.
static const IColor kValueText{255, 188, 188, 188};

// Meter fill color for non-LED meter body/level.
static const IColor kMeterForeground{210, 88, 150, 220};

// Meter marker/hover emphasis color.
static const IColor kMeterHighlight{90, 170, 210, 255};
} // namespace ui

namespace visualizer
{
// Plot background fill.
static const IColor kBackground{255, 8, 10, 12};

// Plot frame/border color.
static const IColor kFrame{255, 42, 48, 56};

// Center horizontal axis line color.
static const IColor kCenterLine{255, 74, 82, 92};

// Minor vertical gridline color.
static const IColor kGridMinor{75, 70, 78, 88};

// Major vertical gridline color.
static const IColor kGridMajor{130, 92, 102, 118};

// X-axis label text color.
static const IColor kLabelText{220, 255, 255, 255};

// Bright white core stroke for harmonic bars.
static const IColor kHarmonicCore{255, 255, 255, 255};

// Harmonic gradient start (low-frequency) to end (high-frequency) colour.
static const IColor kHarmonicGradientStart{255, 0, 100, 255};
static const IColor kHarmonicGradientEnd{255, 255, 100, 0};

// Knobs contain a line that changes colour from min to max value.
static const IColor kKnobMin{255, 255, 178, 128};
static const IColor kKnobMax{255, 128, 178, 255};

// Output meter LED colours ordered from lowest to highest dB range.
inline const std::array<IColor, 26> kOutputMeterLEDColors{{
  {255, 255, 0, 0}, {255, 255, 0, 0}, {255, 255, 178, 128}, {255, 250, 176, 130},
  {255, 244, 174, 134}, {255, 238, 173, 137}, {255, 233, 172, 141}, {255, 227, 171, 145},
  {255, 222, 170, 150}, {255, 216, 169, 156}, {255, 210, 169, 162}, {255, 205, 169, 169},
  {255, 200, 169, 177}, {255, 194, 170, 186}, {255, 186, 170, 194}, {255, 177, 169, 200},
  {255, 169, 169, 205}, {255, 162, 169, 210}, {255, 156, 169, 216}, {255, 150, 170, 222},
  {255, 145, 171, 227}, {255, 141, 172, 233}, {255, 137, 173, 238}, {255, 134, 174, 244},
  {255, 130, 176, 250}, {255, 128, 178, 255}
}};
} // namespace visualizer

namespace editor
{
static const IColor kSurface{255, 8, 10, 12};
static const IColor kTab{255, 24, 32, 42};
static const IColor kSliderBar = ui::kAccentPrimary;
static const IColor kHoverOverlay{55, 255, 255, 255};
static const IColor kUtilityBody{255, 16, 22, 30};
} // namespace editor

// Shared dark background color used across visualizer/editor surfaces.
static const IColor kBackground = visualizer::kBackground;
} // namespace colour
} // namespace plugin_ui

#pragma once

#include "IGraphicsStructs.h"

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

// Harmonic gradient start color (low-frequency side).
static const IColor kHarmonicGradientStart{255, 0, 100, 255};

// Harmonic gradient end color (high-frequency side).
static const IColor kHarmonicGradientEnd{255, 255, 100, 0};

// Bright white core stroke for harmonic bars.
static const IColor kHarmonicCore{255, 255, 255, 255};

// Colours used in the LED output meter.
const static IColor LED01 = {255, 0, 100, 255};
const static IColor LED02 = {255, 11, 100, 244};
const static IColor LED03 = {255, 22, 100, 233};
const static IColor LED04 = {255, 33, 100, 222};
const static IColor LED05 = {255, 44, 100, 211};
const static IColor LED06 = {255, 55, 100, 200};
const static IColor LED07 = {255, 67, 100, 188};
const static IColor LED08 = {255, 78, 100, 177};
const static IColor LED09 = {255, 89, 100, 166};
const static IColor LED10 = {255, 100, 100, 155};
const static IColor LED11 = {255, 111, 100, 144};
const static IColor LED12 = {255, 122, 100, 133};
const static IColor LED13 = {255, 133, 100, 122};
const static IColor LED14 = {255, 144, 100, 111};
const static IColor LED15 = {255, 155, 100, 100};
const static IColor LED16 = {255, 166, 100, 89};
const static IColor LED17 = {255, 177, 100, 78};
const static IColor LED18 = {255, 188, 100, 67};
const static IColor LED19 = {255, 200, 100, 55};
const static IColor LED20 = {255, 211, 100, 44};
const static IColor LED21 = {255, 222, 100, 33};
const static IColor LED22 = {255, 233, 100, 22};
const static IColor LED23 = {255, 244, 100, 11};
const static IColor LED24 = {255, 255, 100, 0};

} // namespace visualizer
} // namespace colour
} // namespace plugin_ui

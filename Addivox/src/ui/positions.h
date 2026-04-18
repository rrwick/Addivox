#pragma once

#include "IControls.h"

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace positions
{
inline const IRECT         kPresetManager = IRECT::MakeXYWH(525,  14, 270,  42);
inline const IRECT      kLoadPresetButton = IRECT::MakeXYWH(795,  14,  50,  42);
inline const IRECT      kSavePresetButton = IRECT::MakeXYWH(845,  14,  50,  42);
inline const IRECT        kSettingsButton = IRECT::MakeXYWH(907,  19,  32,  32);
inline const IRECT           kAboutButton = IRECT::MakeXYWH(945,  19,  32,  32);

inline const IRECT    kHarmonicVisualizer = IRECT::MakeXYWH( 12,  74, 976, 384);
inline const IRECT            kEditorTabs = IRECT::MakeXYWH( 12,  74, 976, 384);

inline const IRECT            kModeSwitch = IRECT::MakeXYWH(872, 467,  42,  26);

inline const IRECT            kAttackKnob = IRECT::MakeXYWH( 80, 480,  50,  60);
inline const IRECT           kReleaseKnob = IRECT::MakeXYWH( 80, 550,  50,  60);

inline const IRECT       kLevelAmountKnob = IRECT::MakeXYWH(150, 480,  50,  60);
inline const IRECT         kLevelRateKnob = IRECT::MakeXYWH(150, 550,  50,  60);
inline const IRECT         kPanAmountKnob = IRECT::MakeXYWH(200, 480,  50,  60);
inline const IRECT           kPanRateKnob = IRECT::MakeXYWH(200, 550,  50,  60);
inline const IRECT       kPitchAmountKnob = IRECT::MakeXYWH(250, 480,  50,  60);
inline const IRECT         kPitchRateKnob = IRECT::MakeXYWH(250, 550,  50,  60);

inline const IRECT        kTransposeLabel = IRECT::MakeXYWH(335, 524,  80,  12);
inline const IRECT    kTransposeNumberBox = IRECT::MakeXYWH(335, 490,  58,  30);
inline const IRECT             kPitchKnob = IRECT::MakeXYWH(400, 480,  50,  60);
inline const IRECT       kPortamentoLabel = IRECT::MakeXYWH(357, 594,  70,  12);
inline const IRECT  kPortamentoMinCaption = IRECT::MakeXYWH(331, 554,  50,  20);
inline const IRECT  kPortamentoMaxCaption = IRECT::MakeXYWH(397, 573,  50,  20);
inline const IRECT kPortamentoRangeSlider = IRECT::MakeXYWH(327, 558, 124,  30);

inline const IRECT               kPanKnob = IRECT::MakeXYWH(500, 480,  50,  60);
inline const IRECT             kLevelKnob = IRECT::MakeXYWH(500, 550,  50,  60);

inline const IRECT             kDriveKnob = IRECT::MakeXYWH(590, 480,  50,  60);
inline const IRECT              kToneKnob = IRECT::MakeXYWH(640, 480,  50,  60);
inline const IRECT            kChorusKnob = IRECT::MakeXYWH(590, 550,  50,  60);
inline const IRECT            kReverbKnob = IRECT::MakeXYWH(640, 550,  50,  60);

inline const IRECT           kBreathMeter = IRECT::MakeXYWH(800, 510, 187,  20);
inline const IRECT           kBreathLabel = IRECT::MakeXYWH(875, 532,  80,  12);
inline const IRECT           kOutputMeter = IRECT::MakeXYWH(800, 553, 187,  46);
inline const IRECT           kOutputLabel = IRECT::MakeXYWH(875, 600,  80,  12);

inline const IRECT        kPitchBendWheel = IRECT::MakeXYWH(  4, 630,  35, 110);
inline const IRECT              kKeyboard = IRECT::MakeXYWH( 38, 630, 952, 110);

inline const IRECT          kAboutContent = IRECT::MakeXYWH(  0,   0, 920, 320);
inline const IRECT             kAboutLogo = IRECT::MakeXYWH(110,  40, 700,  96);
inline const IRECT         kAboutSubtitle = IRECT::MakeXYWH(  0, 160, 920,  30);
inline const IRECT           kAboutUrlRow = IRECT::MakeXYWH(  0, 210, 920,  24);
inline const IRECT          kAboutVersion = IRECT::MakeXYWH(  0, 240, 920,  24);
inline const IRECT        kAboutCopyright = IRECT::MakeXYWH(  0, 270, 920,  24);
inline const IRECT        kAboutBuiltWith = IRECT::MakeXYWH(  0, 300, 920,  24);

} // namespace positions
} // namespace plugin_ui

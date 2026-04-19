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

inline const IRECT    kHarmonicVisualizer = IRECT::MakeXYWH( 12,  74, 976, 385);
inline const IRECT            kEditorTabs = IRECT::MakeXYWH( 12,  74, 976, 385);

inline const IRECT          kVizModeLabel = IRECT::MakeXYWH(826, 468,  40,  25);
inline const IRECT            kModeSwitch = IRECT::MakeXYWH(868, 467,  50,  25);
inline const IRECT         kEditModeLabel = IRECT::MakeXYWH(920, 468,  50,  25);

inline const IRECT            kPitchLabel = IRECT::MakeXYWH(  7, 480,  30, 130);
inline const IRECT        kTransposeLabel = IRECT::MakeXYWH( 43, 524,  80,  12);
inline const IRECT    kTransposeNumberBox = IRECT::MakeXYWH( 43, 490,  58,  30);
inline const IRECT            kTuningKnob = IRECT::MakeXYWH(115, 480,  45,  60);
inline const IRECT       kPortamentoLabel = IRECT::MakeXYWH( 69, 597,  70,  12);
inline const IRECT  kPortamentoMinCaption = IRECT::MakeXYWH( 39, 557,  50,  20);
inline const IRECT  kPortamentoMaxCaption = IRECT::MakeXYWH(112, 576,  50,  20);
inline const IRECT kPortamentoRangeSlider = IRECT::MakeXYWH( 35, 561, 129,  30);

inline const IRECT         kEnvelopeLabel = IRECT::MakeXYWH(175, 480,  30, 130);
inline const IRECT            kAttackKnob = IRECT::MakeXYWH(205, 480,  45,  60);
inline const IRECT           kReleaseKnob = IRECT::MakeXYWH(205, 550,  45,  60);

inline const IRECT        kVariationLabel = IRECT::MakeXYWH(265, 480,  30, 130);
inline const IRECT       kLevelAmountKnob = IRECT::MakeXYWH(295, 480,  45,  60);
inline const IRECT         kLevelRateKnob = IRECT::MakeXYWH(295, 550,  45,  60);
inline const IRECT         kPanAmountKnob = IRECT::MakeXYWH(351, 480,  45,  60);
inline const IRECT           kPanRateKnob = IRECT::MakeXYWH(351, 550,  45,  60);
inline const IRECT       kPitchAmountKnob = IRECT::MakeXYWH(407, 480,  45,  60);
inline const IRECT         kPitchRateKnob = IRECT::MakeXYWH(407, 550,  45,  60);

inline const IRECT           kOutputLabel = IRECT::MakeXYWH(467, 480,  30, 130);
inline const IRECT               kPanKnob = IRECT::MakeXYWH(497, 480,  45,  60);
inline const IRECT             kLevelKnob = IRECT::MakeXYWH(497, 550,  45,  60);

inline const IRECT            kNoiseLabel = IRECT::MakeXYWH(557, 480,  30, 130);
inline const IRECT       kNoiseAttackKnob = IRECT::MakeXYWH(587, 480,  45,  60);
inline const IRECT      kNoiseSustainKnob = IRECT::MakeXYWH(587, 550,  45,  60);

inline const IRECT          kEffectsLabel = IRECT::MakeXYWH(647, 480,  30, 130);
inline const IRECT             kDriveKnob = IRECT::MakeXYWH(677, 480,  45,  60);
inline const IRECT              kToneKnob = IRECT::MakeXYWH(733, 480,  45,  60);
inline const IRECT            kChorusKnob = IRECT::MakeXYWH(677, 550,  45,  60);
inline const IRECT            kReverbKnob = IRECT::MakeXYWH(733, 550,  45,  60);

inline const IRECT           kBreathMeter = IRECT::MakeXYWH(800, 512, 187,  20);
inline const IRECT           kBreathLabel = IRECT::MakeXYWH(835, 534,  38,  12);
inline const IRECT          kMainOutLabel = IRECT::MakeXYWH(910, 546,  50,  12);
inline const IRECT           kOutputMeter = IRECT::MakeXYWH(800, 560, 187,  50);

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

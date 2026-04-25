#pragma once

#include "IControls.h"

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace positions
{
inline const IRECT         kPatchManager = IRECT::MakeXYWH(545,  14, 350,  42);
inline const IRECT      kLoadPatchButton = IRECT::MakeXYWH(795,  14,  50,  42);
inline const IRECT      kSavePatchButton = IRECT::MakeXYWH(845,  14,  50,  42);
inline const IRECT        kSettingsButton = IRECT::MakeXYWH(907,  19,  32,  32);
inline const IRECT           kAboutButton = IRECT::MakeXYWH(945,  19,  32,  32);

inline const IRECT    kHarmonicVisualizer = IRECT::MakeXYWH( 12,  74, 976, 385);
inline const IRECT            kEditorTabs = IRECT::MakeXYWH( 12,  74, 976, 385);

inline const IRECT          kVizModeLabel = IRECT::MakeXYWH(824, 468,  40,  25);
inline const IRECT            kModeSwitch = IRECT::MakeXYWH(866, 467,  50,  25);
inline const IRECT         kEditModeLabel = IRECT::MakeXYWH(918, 468,  50,  25);

inline const IRECT            kPitchLabel = IRECT::MakeXYWH( 12, 480,  30, 130);
inline const IRECT        kTransposeLabel = IRECT::MakeXYWH( 48, 528,  60,  12);
inline const IRECT    kTransposeNumberBox = IRECT::MakeXYWH( 48, 489,  59,  34);
inline const IRECT            kTuningKnob = IRECT::MakeXYWH(120, 480,  50,  60);
inline const IRECT       kPortamentoLabel = IRECT::MakeXYWH( 74, 597,  70,  12);
inline const IRECT  kPortamentoMinCaption = IRECT::MakeXYWH( 44, 557,  50,  20);
inline const IRECT  kPortamentoMaxCaption = IRECT::MakeXYWH(117, 576,  50,  20);
inline const IRECT kPortamentoRangeSlider = IRECT::MakeXYWH( 39, 561, 129,  30);

inline const IRECT         kEnvelopeLabel = IRECT::MakeXYWH(190, 480,  30, 130);
inline const IRECT            kAttackKnob = IRECT::MakeXYWH(225, 480,  50,  60);
inline const IRECT           kReleaseKnob = IRECT::MakeXYWH(225, 550,  50,  60);

inline const IRECT        kVariationLabel = IRECT::MakeXYWH(295, 480,  30, 130);
inline const IRECT       kLevelAmountKnob = IRECT::MakeXYWH(330, 480,  50,  60);
inline const IRECT         kLevelRateKnob = IRECT::MakeXYWH(330, 550,  50,  60);
inline const IRECT         kPanAmountKnob = IRECT::MakeXYWH(394, 480,  50,  60);
inline const IRECT           kPanRateKnob = IRECT::MakeXYWH(394, 550,  50,  60);
inline const IRECT       kPitchAmountKnob = IRECT::MakeXYWH(458, 480,  50,  60);
inline const IRECT         kPitchRateKnob = IRECT::MakeXYWH(458, 550,  50,  60);

inline const IRECT           kOutputLabel = IRECT::MakeXYWH(528, 480,  30, 130);
inline const IRECT               kPanKnob = IRECT::MakeXYWH(563, 480,  50,  60);
inline const IRECT             kLevelKnob = IRECT::MakeXYWH(563, 550,  50,  60);

inline const IRECT          kEffectsLabel = IRECT::MakeXYWH(633, 480,  30, 130);
inline const IRECT             kDriveKnob = IRECT::MakeXYWH(668, 480,  50,  60);
inline const IRECT            kChorusKnob = IRECT::MakeXYWH(668, 550,  50,  60);
inline const IRECT              kToneKnob = IRECT::MakeXYWH(732, 480,  50,  60);
inline const IRECT            kReverbKnob = IRECT::MakeXYWH(732, 550,  50,  60);

inline const IRECT           kBreathMeter = IRECT::MakeXYWH(802, 512, 185,  23);
inline const IRECT           kBreathLabel = IRECT::MakeXYWH(835, 537,  38,  12);
inline const IRECT          kMainOutLabel = IRECT::MakeXYWH(912, 549,  50,  12);
inline const IRECT           kOutputMeter = IRECT::MakeXYWH(802, 563, 185,  47);

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

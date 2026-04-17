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

inline constexpr float kContentWidth = 920;
inline constexpr float kContentHeight = 320;
inline constexpr float kHorizontalInset = 24;
inline constexpr float kDoubleHorizontalInset = 48;
inline constexpr float kUrlRowOffsetY = 210;
inline constexpr float kUrlRowHeight = 24;
inline constexpr float kLogoOffsetY = 30;
inline constexpr float kLogoWidth = 820;
inline constexpr float kLogoHeight = 112;
inline constexpr float kSubtitleOffsetY = 160;
inline constexpr float kSubtitleHeight = 30;
inline constexpr float kVersionOffsetY = 240;
inline constexpr float kMetaRowHeight = 24;
inline constexpr float kCopyrightOffsetY = 270;
inline constexpr float kBuiltWithOffsetY = 300;

inline IRECT GetContentBounds(const IRECT& parentBounds)
{
  return parentBounds.GetCentredInside(kContentWidth, kContentHeight);
}

inline IRECT GetUrlRowBounds(const IRECT& contentBounds)
{
  return IRECT::MakeXYWH(contentBounds.L + kHorizontalInset, contentBounds.T + kUrlRowOffsetY, contentBounds.W() - kDoubleHorizontalInset, kUrlRowHeight);
}

inline IRECT GetLogoBounds(const IRECT& contentBounds)
{
  return IRECT::MakeXYWH(contentBounds.MW() - (kLogoWidth * 0.5f), contentBounds.T + kLogoOffsetY, kLogoWidth, kLogoHeight);
}

inline IRECT GetSubtitleBounds(const IRECT& contentBounds)
{
  return IRECT::MakeXYWH(contentBounds.L, contentBounds.T + kSubtitleOffsetY, contentBounds.W(), kSubtitleHeight);
}

inline IRECT GetUrlTextBounds(const IRECT& urlRowBounds, float textWidth)
{
  return IRECT::MakeXYWH(urlRowBounds.MW() - (textWidth * 0.5f), urlRowBounds.T, textWidth, urlRowBounds.H());
}

inline IRECT GetVersionBounds(const IRECT& contentBounds)
{
  return IRECT::MakeXYWH(contentBounds.L, contentBounds.T + kVersionOffsetY, contentBounds.W(), kMetaRowHeight);
}

inline IRECT GetCopyrightBounds(const IRECT& contentBounds)
{
  return IRECT::MakeXYWH(contentBounds.L + kHorizontalInset, contentBounds.T + kCopyrightOffsetY, contentBounds.W() - kDoubleHorizontalInset, kMetaRowHeight);
}

inline IRECT GetBuiltWithBounds(const IRECT& contentBounds)
{
  return IRECT::MakeXYWH(contentBounds.L, contentBounds.T + kBuiltWithOffsetY, contentBounds.W(), kMetaRowHeight);
}

} // namespace positions
} // namespace plugin_ui

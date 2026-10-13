#pragma once

#include "IControls.h"

#include <algorithm>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace positions
{
inline const IRECT          kPatchManager = IRECT::MakeXYWH(545,  14, 350,  42);
inline const IRECT        kSettingsButton = IRECT::MakeXYWH(907,  19,  32,  32);
inline const IRECT           kAboutButton = IRECT::MakeXYWH(945,  19,  32,  32);

inline const IRECT    kHarmonicVisualizer = IRECT::MakeXYWH( 12,  74, 976, 385);
inline const IRECT            kEditorTabs = IRECT::MakeXYWH( 12,  74, 976, 385);

inline const IRECT          kVizModeLabel = IRECT::MakeXYWH(837, 468,  40,  25);
inline const IRECT            kModeSwitch = IRECT::MakeXYWH(879, 467,  50,  25);
inline const IRECT         kEditModeLabel = IRECT::MakeXYWH(931, 468,  50,  25);

// Knob panels share these spacings, so a panel with n knob columns is 37 + 54n px wide. background.svg draws the panels at the same positions.
// Knob labels are wider than the knobs, so columns are spaced for the labels and the knobs overhang their columns slightly.
namespace knob_panel
{
inline constexpr float            kTop = 470.f;
inline constexpr float         kHeight = 146.f;
inline constexpr float       kPanelGap = 6.f;
inline constexpr float    kEdgeToLabel = 8.f;
inline constexpr float     kLabelWidth = 16.f;
inline constexpr float  kLabelToColumn = 13.f;
inline constexpr float    kColumnWidth = 40.f;
inline constexpr float      kColumnGap = 14.f;
inline constexpr float   kColumnToEdge = 14.f;
inline constexpr float  kColumnsOffset = kEdgeToLabel + kLabelWidth + kLabelToColumn;
inline constexpr float       kKnobSize = 46.f;
inline constexpr float kKnobCellHeight = kKnobSize + 14.f; // Knob, 2 px gap and 12 px label (see LabelledKnob).
inline constexpr float        kRowsTop = kTop + 9.f;
inline constexpr float     kRowSpacing = kKnobCellHeight + 8.f;

constexpr float Width(int numColumns) { return kColumnsOffset + (numColumns * kColumnWidth) + ((numColumns - 1) * kColumnGap) + kColumnToEdge; }

constexpr float LabelTop(int row) { return kRowsTop + (row * kRowSpacing) + kKnobSize + 2.f; }

inline IRECT SectionLabel(float panelX) { return IRECT::MakeXYWH(panelX, kTop, 2.f * (kEdgeToLabel + (0.5f * kLabelWidth)), kHeight); }

// Knob bounds also hold the label, which is clipped to them, so they extend symmetrically into the space beside the column.
inline IRECT KnobAt(float columnX, float labelOverhang, int row) {
  return IRECT::MakeXYWH(columnX - labelOverhang, kRowsTop + (row * kRowSpacing), kColumnWidth + (2.f * labelOverhang), kKnobCellHeight);
}

inline IRECT Knob(float panelX, int numColumns, int column, int row) {
  const float labelOverhang = (numColumns == 1) ? std::min(kLabelToColumn, kColumnToEdge) : (0.5f * kColumnGap);
  return KnobAt(panelX + kColumnsOffset + (column * (kColumnWidth + kColumnGap)), labelOverhang, row);
}
} // namespace knob_panel

inline constexpr float    kTransposeWidth = 59.f;
inline constexpr float   kTransposeHeight = 34.f;
inline constexpr float kPitchContentWidth = kTransposeWidth + knob_panel::kColumnGap + knob_panel::kColumnWidth;
inline constexpr float       kPitchPanelX = 7.f;
inline constexpr float     kPitchContentX = kPitchPanelX + knob_panel::kColumnsOffset;
inline constexpr float    kEnvelopePanelX = kPitchContentX + kPitchContentWidth + knob_panel::kColumnToEdge + knob_panel::kPanelGap;
inline constexpr float   kVariationPanelX = kEnvelopePanelX + knob_panel::Width(1) + knob_panel::kPanelGap;
inline constexpr float          kEqPanelX = kVariationPanelX + knob_panel::Width(3) + knob_panel::kPanelGap;
inline constexpr float      kOutputPanelX = kEqPanelX + knob_panel::Width(1) + knob_panel::kPanelGap;
inline constexpr float     kEffectsPanelX = kOutputPanelX + knob_panel::Width(1) + knob_panel::kPanelGap;
inline constexpr float       kMeterPanelX = kEffectsPanelX + knob_panel::Width(2) + knob_panel::kPanelGap;
static_assert(kMeterPanelX == 824.f, "Update the panels in background.svg to match");

inline const IRECT            kPitchLabel = knob_panel::SectionLabel(kPitchPanelX);
inline const IRECT    kTransposeNumberBox = IRECT::MakeXYWH(kPitchContentX, knob_panel::kRowsTop + (0.5f * (knob_panel::kKnobSize - kTransposeHeight)),
                                                            kTransposeWidth, kTransposeHeight);
inline const IRECT        kTransposeLabel = IRECT::MakeXYWH(kPitchContentX, knob_panel::LabelTop(0), 60, 12);
inline const IRECT            kTuningKnob = knob_panel::KnobAt(kPitchContentX + kTransposeWidth + knob_panel::kColumnGap, 0.5f * knob_panel::kColumnGap, 0);
inline const IRECT kPortamentoRangeSlider = IRECT::MakeXYWH(kPitchContentX - 8, knob_panel::LabelTop(1) - 36, kPitchContentWidth + 16, 30);
inline const IRECT  kPortamentoMinCaption = IRECT::MakeXYWH(kPortamentoRangeSlider.L + 5, knob_panel::LabelTop(1) - 40, 50, 20);
inline const IRECT  kPortamentoMaxCaption = IRECT::MakeXYWH(kPortamentoRangeSlider.R - 51, knob_panel::LabelTop(1) - 21, 50, 20);
inline const IRECT       kPortamentoLabel = IRECT::MakeXYWH(kPitchContentX + (0.5f * kPitchContentWidth) - 32, knob_panel::LabelTop(1), 70, 12);

inline const IRECT         kEnvelopeLabel = knob_panel::SectionLabel(kEnvelopePanelX);
inline const IRECT            kAttackKnob = knob_panel::Knob(kEnvelopePanelX, 1, 0, 0);
inline const IRECT           kReleaseKnob = knob_panel::Knob(kEnvelopePanelX, 1, 0, 1);

inline const IRECT        kVariationLabel = knob_panel::SectionLabel(kVariationPanelX);
inline const IRECT       kLevelAmountKnob = knob_panel::Knob(kVariationPanelX, 3, 0, 0);
inline const IRECT         kLevelRateKnob = knob_panel::Knob(kVariationPanelX, 3, 0, 1);
inline const IRECT         kPanAmountKnob = knob_panel::Knob(kVariationPanelX, 3, 1, 0);
inline const IRECT           kPanRateKnob = knob_panel::Knob(kVariationPanelX, 3, 1, 1);
inline const IRECT       kPitchAmountKnob = knob_panel::Knob(kVariationPanelX, 3, 2, 0);
inline const IRECT         kPitchRateKnob = knob_panel::Knob(kVariationPanelX, 3, 2, 1);

inline const IRECT               kEqLabel = knob_panel::SectionLabel(kEqPanelX);
inline const IRECT          kFormantsKnob = knob_panel::Knob(kEqPanelX, 1, 0, 0);
inline const IRECT         kResonanceKnob = knob_panel::Knob(kEqPanelX, 1, 0, 1);

inline const IRECT           kOutputLabel = knob_panel::SectionLabel(kOutputPanelX);
inline const IRECT               kPanKnob = knob_panel::Knob(kOutputPanelX, 1, 0, 0);
inline const IRECT             kLevelKnob = knob_panel::Knob(kOutputPanelX, 1, 0, 1);

inline const IRECT          kEffectsLabel = knob_panel::SectionLabel(kEffectsPanelX);
inline const IRECT             kDriveKnob = knob_panel::Knob(kEffectsPanelX, 2, 0, 0);
inline const IRECT            kChorusKnob = knob_panel::Knob(kEffectsPanelX, 2, 0, 1);
inline const IRECT              kToneKnob = knob_panel::Knob(kEffectsPanelX, 2, 1, 0);
inline const IRECT            kReverbKnob = knob_panel::Knob(kEffectsPanelX, 2, 1, 1);

inline const IRECT           kBreathMeter = IRECT::MakeXYWH(830, 512, 157,  23);
inline const IRECT           kBreathLabel = IRECT::MakeXYWH(857, 537,  38,  12);
inline const IRECT          kMainOutLabel = IRECT::MakeXYWH(918, 549,  50,  12);
inline const IRECT           kOutputMeter = IRECT::MakeXYWH(830, 563, 157,  47);

inline const IRECT        kPitchBendWheel = IRECT::MakeXYWH(  4, 630,  35, 110);
inline const IRECT              kKeyboard = IRECT::MakeXYWH( 38, 630, 952, 110);

inline const IRECT          kAboutContent = IRECT::MakeXYWH(  0,   0, 920, 320);
inline const IRECT             kAboutLogo = IRECT::MakeXYWH(110,  40, 700,  96);
inline const IRECT         kAboutSubtitle = IRECT::MakeXYWH(  0, 160, 920,  30);
inline const IRECT           kAboutUrlRow = IRECT::MakeXYWH(  0, 210, 920,  24);
inline const IRECT          kAboutVersion = IRECT::MakeXYWH(  0, 240, 920,  24);
inline const IRECT        kAboutCopyright = IRECT::MakeXYWH(  0, 270, 920,  24);
inline const IRECT        kAboutBuiltWith = IRECT::MakeXYWH(  0, 300, 920,  24);
inline const IRECT         kAboutDemoText = IRECT::MakeXYWH(  0, 330, 920,  24);

} // namespace positions
} // namespace plugin_ui

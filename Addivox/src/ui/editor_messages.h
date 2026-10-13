#pragma once

#include <array>
#include <utility>

#include "../midi/breath_control.h"
#include "../settings/oscillator.h"
#include "IPlugStructs.h"

namespace editor_messages {
inline constexpr int       kMsgTagSetKeyNoteOscillatorParameter = 1000;
inline constexpr int                     kMsgTagAddKeyNotePatch = 1001;
inline constexpr int                  kMsgTagRemoveKeyNotePatch = 1002;
inline constexpr int kMsgTagSetKeyNoteOscillatorParameterValues = 1003;
inline constexpr int             kMsgTagPromptLoadPatchFromFile = 1004;
inline constexpr int               kMsgTagPromptSavePatchToFile = 1005;
inline constexpr int                   kMsgTagSetKeyNoteEqCurve = 1006;
inline constexpr int               kMsgTagSetAllKeyNotesEnabled = 1007;
inline constexpr int             kMsgTagSetAllKeyNotesEqEnabled = 1008;
inline constexpr int                   kMsgTagSetBreathCCSource = 1009;
inline constexpr int        kMsgTagSetHarmonicVisualizerEnabled = 1010;
inline constexpr int      kMsgTagResetStandaloneStateToDefaults = 1011;
inline constexpr int                  kMsgTagPatchManagerAction = 1012;
inline constexpr int               kMsgTagOpenAudioMidiSettings = 1013;
inline constexpr int              kMsgTagSetPitchBendRange = 1014;
inline constexpr int                    kMsgTagSetPortamentoCC = 1015;
inline constexpr int                kMsgTagSetKeyNoteResonance = 1016;

enum class PatchManagerAction { SelectPatch = 0, PreviousPatch, NextPatch, SavePatch, ImportPatch, ImportCollection, ShowPatchInFileBrowser, RefreshPatches };

struct PatchManagerActionPayload {
  int action{static_cast<int>(PatchManagerAction::SelectPatch)};
  int patchId{-1};
};

struct SetKeyNoteOscillatorParameterPayload {
  int midiNote{0};
  int oscillatorIndex{0};
  int parameter{0};
  double value{0.0};
};

struct KeyNotePatchPayload {
  int midiNote{0};
};

struct SetKeyNoteOscillatorParameterValuesPayload {
  int midiNote{0};
  int parameter{0};
  std::array<double, SimplePatch::kNumOscillators> values{};
};

struct SetAllKeyNotesEnabledPayload {
  int parameter{0};
  int enabled{0};
  int midiNote{0};
};

struct SetAllKeyNotesEqEnabledPayload {
  int enabled{0};
};

struct SetBreathCCSourcePayload {
  int source{static_cast<int>(kDefaultBreathCCSource)};
};

struct SetPortamentoCCPayload {
  int controller{5};
};

struct SetPitchBendRangePayload {
  int semitones{2};
};

struct SetHarmonicVisualizerEnabledPayload {
  int enabled{1};
};

struct EqCurvePointPayload {
  double frequencyHz{0.0};
  double gainDb{0.0};
};

inline void PutCurvePoints(iplug::IByteChunk& chunk, const EqCurve::PointList& points) {
  const int numPoints = static_cast<int>(points.size());
  chunk.Put(&numPoints);
  for (const auto& point : points) {
    const EqCurvePointPayload payload{point.frequencyHz, point.gainDb};
    chunk.Put(&payload);
  }
}

// Returns the position after the points, or -1 if the stream is too short. A negative startPos propagates, so reads can be chained.
inline int GetCurvePoints(const iplug::IByteStream& stream, int startPos, EqCurve::PointList& points) {
  int numPoints = 0;
  int position = stream.Get(&numPoints, startPos);
  for (int i = 0; i < numPoints && position >= 0; ++i) {
    EqCurvePointPayload payload;
    position = stream.Get(&payload, position);
    points.push_back({payload.frequencyHz, payload.gainDb});
  }
  return position;
}

inline void SerializeKeyNoteEqCurvePayload(int midiNote, const EqCurve& curve, iplug::IByteChunk& chunk) {
  chunk.Clear();
  chunk.Put(&midiNote);
  PutCurvePoints(chunk, curve.GetPoints());
}

inline bool DeserializeKeyNoteEqCurvePayload(int dataSize, const void* pData, int& midiNote, EqCurve& curve) {
  const iplug::IByteStream stream{pData, dataSize};
  EqCurve::PointList points;
  if (!pData || GetCurvePoints(stream, stream.Get(&midiNote, 0), points) < 0) return false;

  curve = EqCurve{std::move(points)};
  return true;
}

inline void SerializeKeyNoteResonancePayload(int midiNote, const ResonanceSettings& resonance, iplug::IByteChunk& chunk) {
  chunk.Clear();
  chunk.Put(&midiNote);
  PutCurvePoints(chunk, resonance.curves.GetPoints(ResonanceCurves::Breath::zero));
  PutCurvePoints(chunk, resonance.curves.GetPoints(ResonanceCurves::Breath::full));
  chunk.Put(&resonance.breathResponse);
  chunk.Put(&resonance.tracking);
}

inline bool DeserializeKeyNoteResonancePayload(int dataSize, const void* pData, int& midiNote, ResonanceSettings& resonance) {
  const iplug::IByteStream stream{pData, dataSize};
  EqCurve::PointList zeroBreathPoints;
  EqCurve::PointList fullBreathPoints;
  int position = GetCurvePoints(stream, GetCurvePoints(stream, stream.Get(&midiNote, 0), zeroBreathPoints), fullBreathPoints);
  position = stream.Get(&resonance.tracking, stream.Get(&resonance.breathResponse, position));
  if (!pData || position < 0) return false;

  resonance.Sanitize();
  return resonance.curves.SetPoints(std::move(zeroBreathPoints), std::move(fullBreathPoints));
}
} // namespace editor_messages

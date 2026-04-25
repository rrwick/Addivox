#pragma once

#include <array>
#include <utility>

#include "IPlugStructs.h"
#include "../midi/breath_control.h"
#include "../settings/oscillator.h"

namespace editor_messages
{
inline constexpr int kMsgTagSetKeyNoteOscillatorParameter = 1000;
inline constexpr int kMsgTagAddKeyNotePatch = 1001;
inline constexpr int kMsgTagRemoveKeyNotePatch = 1002;
inline constexpr int kMsgTagSetKeyNoteOscillatorParameterValues = 1003;
inline constexpr int kMsgTagPromptLoadPatchFromFile = 1004;
inline constexpr int kMsgTagPromptSavePatchToFile = 1005;
inline constexpr int kMsgTagSetKeyNoteEqCurve = 1006;
inline constexpr int kMsgTagSetAllKeyNotesEnabled = 1007;
inline constexpr int kMsgTagSetAllKeyNotesEqEnabled = 1008;
inline constexpr int kMsgTagSetBreathCCSource = 1009;
inline constexpr int kMsgTagSetHarmonicVisualizerEnabled = 1010;
inline constexpr int kMsgTagResetStandaloneStateToDefaults = 1011;
inline constexpr int kMsgTagPatchManagerAction = 1012;

enum class PatchManagerAction
{
  SelectPatch = 0,
  PreviousPatch,
  NextPatch,
  SavePatch,
  ImportPatch,
  ImportCollection,
  ShowPatchInFileBrowser,
  RefreshPatches
};

struct PatchManagerActionPayload
{
  int action{static_cast<int>(PatchManagerAction::SelectPatch)};
  int patchId{-1};
};

struct SetKeyNoteOscillatorParameterPayload
{
  int midiNote{0};
  int oscillatorIndex{0};
  int parameter{0};
  double value{0.0};
};

struct KeyNotePatchPayload
{
  int midiNote{0};
};

struct SetKeyNoteOscillatorParameterValuesPayload
{
  int midiNote{0};
  int parameter{0};
  std::array<double, SimplePatch::kNumOscillators> values{};
};

struct SetAllKeyNotesEnabledPayload
{
  int parameter{0};
  int enabled{0};
  int midiNote{0};
};

struct SetAllKeyNotesEqEnabledPayload
{
  int enabled{0};
};

struct SetBreathCCSourcePayload
{
  int source{static_cast<int>(kDefaultBreathCCSource)};
};

struct SetHarmonicVisualizerEnabledPayload
{
  int enabled{1};
};

struct EqCurvePointPayload
{
  double frequencyHz{0.0};
  double gainDb{0.0};
};

inline void SerializeKeyNoteEqCurvePayload(int midiNote, const EqCurve& curve, iplug::IByteChunk& chunk)
{
  chunk.Clear();
  chunk.Put(&midiNote);
  const int numPoints = static_cast<int>(curve.GetPoints().size());
  chunk.Put(&numPoints);
  for(const auto& point : curve.GetPoints())
  {
    const EqCurvePointPayload payload{point.frequencyHz, point.gainDb};
    chunk.Put(&payload);
  }
}

inline bool DeserializeKeyNoteEqCurvePayload(int dataSize,
                                             const void* pData,
                                             int& midiNote,
                                             EqCurve& curve)
{
  if(dataSize < static_cast<int>(sizeof(int) * 2) || !pData)
    return false;

  iplug::IByteChunk chunk;
  chunk.PutBytes(pData, dataSize);

  int position = 0;
  position = chunk.Get(&midiNote, position);
  if(position < 0)
    return false;

  int numPoints = 0;
  position = chunk.Get(&numPoints, position);
  if(position < 0 || numPoints < 0)
    return false;

  EqCurve::PointList points;
  points.reserve(static_cast<std::size_t>(numPoints));
  for(int i = 0; i < numPoints; ++i)
  {
    EqCurvePointPayload payload;
    position = chunk.Get(&payload, position);
    if(position < 0)
      return false;

    points.push_back({payload.frequencyHz, payload.gainDb});
  }

  curve = EqCurve{std::move(points)};
  return true;
}
} // namespace editor_messages

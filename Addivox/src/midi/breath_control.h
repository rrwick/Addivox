#pragma once

#include "IPlugMidi.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>

enum class BreathCCSource : uint8_t { ModWheel = 0, Breath, BreathHighRes, Volume, VolumeHighRes, Expression, ExpressionHighRes, kCount };

inline constexpr BreathCCSource kDefaultBreathCCSource = BreathCCSource::Breath;

inline constexpr int kNoBreathCCController = -1;

struct BreathCCSourceInfo {
  BreathCCSource source;
  const char* menuLabel;
  int msbController;
  int lsbController; // kNoBreathCCController unless the source is high-resolution

  bool IsHighResolution() const { return lsbController != kNoBreathCCController; }
};

inline constexpr std::array<BreathCCSourceInfo, static_cast<std::size_t>(BreathCCSource::kCount)> kBreathCCSourceInfos{{
    {BreathCCSource::ModWheel,          "CC 1 (Mod Wheel)",                    1,  kNoBreathCCController},
    {BreathCCSource::Breath,            "CC 2 (Breath)",                       2,  kNoBreathCCController},
    {BreathCCSource::BreathHighRes,     "CC 2 + CC 34 (High-Res Breath)",      2,  34},
    {BreathCCSource::Volume,            "CC 7 (Volume)",                       7,  kNoBreathCCController},
    {BreathCCSource::VolumeHighRes,     "CC 7 + CC 39 (High-Res Volume)",      7,  39},
    {BreathCCSource::Expression,        "CC 11 (Expression)",                  11, kNoBreathCCController},
    {BreathCCSource::ExpressionHighRes, "CC 11 + CC 43 (High-Res Expression)", 11, 43},
}};

inline BreathCCSource SanitizeBreathCCSource(int rawValue) {
  if (rawValue < 0 || rawValue >= static_cast<int>(BreathCCSource::kCount)) return kDefaultBreathCCSource;

  return static_cast<BreathCCSource>(rawValue);
}

inline const BreathCCSourceInfo& GetBreathCCSourceInfo(BreathCCSource source) {
  return kBreathCCSourceInfos[static_cast<std::size_t>(SanitizeBreathCCSource(static_cast<int>(source)))];
}

inline bool TryParseBreathCCSourceMenuLabel(const char* text, BreathCCSource& source) {
  if (!text) return false;

  for (const BreathCCSourceInfo& info : kBreathCCSourceInfos) {
    if (std::strcmp(text, info.menuLabel) == 0) {
      source = info.source;
      return true;
    }
  }

  return false;
}

inline bool IsSupportedBreathCCController(int controller) {
  switch (controller) {
  case 1:
  case 2:
  case 7:
  case 11:
  case 34:
  case 39:
  case 43: return true;
  default: return false;
  }
}

inline double GetHighResolutionBreathValue(int msbValue, int lsbValue) {
  const int msb = std::clamp(msbValue, 0, 127);
  const int lsb = std::clamp(lsbValue, 0, 127);
  return (static_cast<double>(msb) * 128.0 + static_cast<double>(lsb)) / 16383.0;
}

inline std::optional<BreathCCSource> DetectBreathCCSource(const std::array<bool, 128>& seenControllers) {
  if (seenControllers[2] && seenControllers[34]) return BreathCCSource::BreathHighRes;
  if (seenControllers[2]) return BreathCCSource::Breath;
  if (seenControllers[11] && seenControllers[43]) return BreathCCSource::ExpressionHighRes;
  if (seenControllers[11]) return BreathCCSource::Expression;
  if (seenControllers[7] && seenControllers[39]) return BreathCCSource::VolumeHighRes;
  if (seenControllers[7]) return BreathCCSource::Volume;
  if (seenControllers[1]) return BreathCCSource::ModWheel;

  return std::nullopt;
}

inline iplug::IMidiMsg MakeControlChangeMsg(int controllerNumber, int controllerValue, int channel, int offset) {
  iplug::IMidiMsg msg;
  msg.mStatus = static_cast<uint8_t>((std::clamp(channel, 0, 15) & 0x0F) | (iplug::IMidiMsg::kControlChange << 4));
  msg.mData1 = static_cast<uint8_t>(std::clamp(controllerNumber, 0, 127));
  msg.mData2 = static_cast<uint8_t>(std::clamp(controllerValue, 0, 127));
  msg.mOffset = offset;
  return msg;
}

struct BreathCCValueUpdate {
  bool consumed{false};
  bool hasValue{false};
  double value{1.0};
};

class BreathCCInputTracker {
public:
  void Reset() { mChannels.fill(ChannelState{}); }

  void ResetChannel(int channel) {
    mChannels[static_cast<std::size_t>(std::clamp(channel, 0, kNumMidiChannels - 1))] = ChannelState{};
  }

  BreathCCValueUpdate HandleMessage(BreathCCSource source, const iplug::IMidiMsg& msg) {
    BreathCCValueUpdate update;

    if (msg.StatusMsg() != iplug::IMidiMsg::kControlChange) return update;

    const int controller = static_cast<int>(msg.mData1);
    if (!IsSupportedBreathCCController(controller)) return update;

    update.consumed = true;

    const int value = std::clamp(static_cast<int>(msg.mData2), 0, 127);
    const BreathCCSourceInfo& info = GetBreathCCSourceInfo(source);

    if (!info.IsHighResolution()) {
      if (controller == info.msbController) {
        update.hasValue = true;
        update.value = static_cast<double>(value) / 127.0;
      }
      return update;
    }

    ChannelState& state = mChannels[static_cast<std::size_t>(std::clamp(msg.Channel(), 0, kNumMidiChannels - 1))];

    if (controller == info.msbController) {
      state.msbValue = value;
      state.hasMSB = true;
      update.hasValue = true;
      update.value = GetHighResolutionBreathValue(value, state.hasLSB ? state.lsbValue : 0);
    } else if (controller == info.lsbController) {
      state.lsbValue = value;
      state.hasLSB = true;
      if (state.hasMSB) {
        update.hasValue = true;
        update.value = GetHighResolutionBreathValue(state.msbValue, state.lsbValue);
      }
    }

    return update;
  }

private:
  static constexpr int kNumMidiChannels = 16;

  struct ChannelState {
    int msbValue{127};
    int lsbValue{0};
    bool hasMSB{false};
    bool hasLSB{false};
  };

  std::array<ChannelState, kNumMidiChannels> mChannels{};
};

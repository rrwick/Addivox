#pragma once

#include "IPlugMidi.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>

enum class BreathCCSource : uint8_t { ModWheel = 0, Breath, BreathHighRes, Volume, VolumeHighRes, Expression, ExpressionHighRes, kCount };

inline constexpr BreathCCSource kDefaultBreathCCSource = BreathCCSource::Breath;

inline constexpr std::array<BreathCCSource, static_cast<std::size_t>(BreathCCSource::kCount)> kBreathCCSources{{
    BreathCCSource::ModWheel,
    BreathCCSource::Breath,
    BreathCCSource::BreathHighRes,
    BreathCCSource::Volume,
    BreathCCSource::VolumeHighRes,
    BreathCCSource::Expression,
    BreathCCSource::ExpressionHighRes,
}};

inline const char* GetBreathCCSourceMenuLabel(BreathCCSource source) {
  switch (source) {
  case BreathCCSource::ModWheel:          return "CC 1 (Mod Wheel)";
  case BreathCCSource::Breath:            return "CC 2 (Breath)";
  case BreathCCSource::BreathHighRes:     return "CC 2 + CC 34 (High-Res Breath)";
  case BreathCCSource::Volume:            return "CC 7 (Volume)";
  case BreathCCSource::VolumeHighRes:     return "CC 7 + CC 39 (High-Res Volume)";
  case BreathCCSource::Expression:        return "CC 11 (Expression)";
  case BreathCCSource::ExpressionHighRes: return "CC 11 + CC 43 (High-Res Expression)";
  default:                                return GetBreathCCSourceMenuLabel(kDefaultBreathCCSource);
  }
}

inline bool TryParseBreathCCSourceMenuLabel(const char* text, BreathCCSource& source) {
  if (!text) return false;

  for (const BreathCCSource candidate : kBreathCCSources) {
    if (std::strcmp(text, GetBreathCCSourceMenuLabel(candidate)) == 0) {
      source = candidate;
      return true;
    }
  }

  return false;
}

inline bool IsHighResolutionBreathCCSource(BreathCCSource source) {
  switch (source) {
  case BreathCCSource::BreathHighRes:
  case BreathCCSource::VolumeHighRes:
  case BreathCCSource::ExpressionHighRes: return true;
  default:                                return false;
  }
}

inline int GetBreathCCSourceMSBController(BreathCCSource source) {
  switch (source) {
  case BreathCCSource::ModWheel:          return 1;
  case BreathCCSource::Breath:
  case BreathCCSource::BreathHighRes:     return 2;
  case BreathCCSource::Volume:
  case BreathCCSource::VolumeHighRes:     return 7;
  case BreathCCSource::Expression:
  case BreathCCSource::ExpressionHighRes: return 11;
  default:                                return GetBreathCCSourceMSBController(kDefaultBreathCCSource);
  }
}

inline int GetBreathCCSourceLSBController(BreathCCSource source) {
  switch (source) {
  case BreathCCSource::BreathHighRes:     return 34;
  case BreathCCSource::VolumeHighRes:     return 39;
  case BreathCCSource::ExpressionHighRes: return 43;
  default:                                return -1;
  }
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

inline BreathCCSource SanitizeBreathCCSource(int rawValue) {
  if (rawValue < 0 || rawValue >= static_cast<int>(BreathCCSource::kCount)) return kDefaultBreathCCSource;

  return static_cast<BreathCCSource>(rawValue);
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
  void Reset() {
    for (int channel = 0; channel < kNumMidiChannels; ++channel) ResetChannel(channel);
  }

  void ResetChannel(int channel) {
    ChannelState& state = mChannels[static_cast<std::size_t>(std::clamp(channel, 0, kNumMidiChannels - 1))];
    state.msbValue = 127;
    state.lsbValue = 0;
    state.hasMSB = false;
    state.hasLSB = false;
  }

  BreathCCValueUpdate HandleMessage(BreathCCSource source, const iplug::IMidiMsg& msg) {
    BreathCCValueUpdate update;

    if (msg.StatusMsg() != iplug::IMidiMsg::kControlChange) return update;

    const int controller = static_cast<int>(msg.mData1);
    if (!IsSupportedBreathCCController(controller)) return update;

    update.consumed = true;

    const int channel = std::clamp(msg.Channel(), 0, kNumMidiChannels - 1);
    const int value = std::clamp(static_cast<int>(msg.mData2), 0, 127);
    const int msbController = GetBreathCCSourceMSBController(source);

    if (!IsHighResolutionBreathCCSource(source)) {
      if (controller == msbController) {
        update.hasValue = true;
        update.value = static_cast<double>(value) / 127.0;
      }
      return update;
    }

    ChannelState& state = mChannels[static_cast<std::size_t>(channel)];
    const int lsbController = GetBreathCCSourceLSBController(source);

    if (controller == msbController) {
      state.msbValue = value;
      state.hasMSB = true;
      update.hasValue = true;
      update.value = GetHighResolutionBreathValue(value, state.hasLSB ? state.lsbValue : 0);
      return update;
    }

    if (controller == lsbController) {
      state.lsbValue = value;
      state.hasLSB = true;
      if (state.hasMSB) {
        update.hasValue = true;
        update.value = GetHighResolutionBreathValue(state.msbValue, state.lsbValue);
      }
      return update;
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

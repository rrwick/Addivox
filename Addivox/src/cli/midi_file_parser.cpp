#include "midi_file_parser.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace midi_file {
namespace {
constexpr uint32_t kDefaultTempoMicrosecondsPerQuarterNote = 500000;

struct RawEvent {
  uint64_t tick{0};
  uint64_t sequence{0};
  iplug::IMidiMsg message{};
};

struct TempoEvent {
  uint64_t tick{0};
  uint64_t sequence{0};
  uint32_t microsecondsPerQuarterNote{kDefaultTempoMicrosecondsPerQuarterNote};
};

struct ActiveNote {
  int channel{0};
  int note{0};
};

class ByteReader {
public:
  explicit ByteReader(const std::vector<uint8_t>& bytes) : mBytes(bytes) {}

  std::size_t Position() const { return mPosition; }

  std::size_t Remaining() const { return mBytes.size() - mPosition; }

  bool CanRead(std::size_t byteCount) const { return byteCount <= Remaining(); }

  bool Skip(std::size_t byteCount) {
    if (!CanRead(byteCount)) return false;

    mPosition += byteCount;
    return true;
  }

  bool ReadByte(uint8_t& value) {
    if (!CanRead(1)) return false;

    value = mBytes[mPosition++];
    return true;
  }

  bool ReadUint16BE(uint16_t& value) {
    if (!CanRead(2)) return false;

    value = static_cast<uint16_t>((static_cast<uint16_t>(mBytes[mPosition]) << 8) | static_cast<uint16_t>(mBytes[mPosition + 1]));
    mPosition += 2;
    return true;
  }

  bool ReadUint32BE(uint32_t& value) {
    if (!CanRead(4)) return false;

    value = (static_cast<uint32_t>(mBytes[mPosition]) << 24) | (static_cast<uint32_t>(mBytes[mPosition + 1]) << 16) |
            (static_cast<uint32_t>(mBytes[mPosition + 2]) << 8) | static_cast<uint32_t>(mBytes[mPosition + 3]);
    mPosition += 4;
    return true;
  }

  bool ReadChunkId(char (&chunkId)[5]) {
    if (!CanRead(4)) return false;

    for (int index = 0; index < 4; ++index) chunkId[index] = static_cast<char>(mBytes[mPosition + static_cast<std::size_t>(index)]);

    chunkId[4] = '\0';
    mPosition += 4;
    return true;
  }

  bool ReadVariableLengthQuantity(uint32_t& value) {
    value = 0;

    for (int index = 0; index < 4; ++index) {
      uint8_t byte = 0;
      if (!ReadByte(byte)) return false;

      value = (value << 7) | static_cast<uint32_t>(byte & 0x7Fu);
      if ((byte & 0x80u) == 0u) return true;
    }

    return false;
  }

private:
  const std::vector<uint8_t>& mBytes;
  std::size_t mPosition{0};
};

bool Fail(std::string* errorMessage, std::string message) {
  if (errorMessage) *errorMessage = std::move(message);
  return false;
}

bool ReadBinaryFile(std::string_view path, std::vector<uint8_t>& bytes, std::string* errorMessage) {
  std::ifstream stream{std::string{path}, std::ios::binary | std::ios::ate};
  if (!stream.is_open()) return Fail(errorMessage, "Could not open MIDI file");

  const std::ifstream::pos_type endPosition = stream.tellg();
  if (endPosition < 0) return Fail(errorMessage, "Could not determine MIDI file size");

  bytes.resize(static_cast<std::size_t>(endPosition));
  stream.seekg(0, std::ios::beg);
  if (!bytes.empty()) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

  if (!stream.good() && !stream.eof()) return Fail(errorMessage, "Could not read MIDI file");

  return true;
}

bool IsNoteOnMessage(const iplug::IMidiMsg& message) { return message.StatusMsg() == iplug::IMidiMsg::kNoteOn && message.Velocity() > 0; }

bool IsNoteOffMessage(const iplug::IMidiMsg& message) {
  if (message.StatusMsg() == iplug::IMidiMsg::kNoteOff) return true;

  return message.StatusMsg() == iplug::IMidiMsg::kNoteOn && message.Velocity() == 0;
}

int EventPriority(const iplug::IMidiMsg& message) {
  if (IsNoteOffMessage(message)) return 1;
  if (IsNoteOnMessage(message)) return 2;
  return 0;
}

double TicksToSeconds(uint64_t tickDelta, uint32_t microsecondsPerQuarterNote, uint16_t ticksPerQuarterNote) {
  return (static_cast<double>(tickDelta) * static_cast<double>(microsecondsPerQuarterNote)) / (1000000.0 * static_cast<double>(ticksPerQuarterNote));
}

iplug::IMidiMsg MakeNoteOffMessage(int channel, int note) {
  return iplug::IMidiMsg{0, static_cast<uint8_t>(0x80 | (channel & 0x0F)), static_cast<uint8_t>(note & 0x7F), 0};
}

bool ParseTrack(ByteReader& reader, std::size_t trackEndPosition, std::vector<RawEvent>& rawEvents, std::vector<TempoEvent>& tempoEvents,
                uint64_t& nextSequence, bool& trackContainsNoteEvents, uint64_t& trackEndTick, std::string* errorMessage) {
  uint64_t absoluteTick = 0;
  uint8_t runningStatus = 0;
  trackContainsNoteEvents = false;

  while (reader.Position() < trackEndPosition) {
    uint32_t deltaTicks = 0;
    if (!reader.ReadVariableLengthQuantity(deltaTicks)) return Fail(errorMessage, "Malformed MIDI track: invalid delta time");

    absoluteTick += static_cast<uint64_t>(deltaTicks);

    uint8_t statusOrData = 0;
    if (!reader.ReadByte(statusOrData)) return Fail(errorMessage, "Malformed MIDI track: unexpected end of track");

    uint8_t statusByte = statusOrData;
    uint8_t firstDataByte = 0;
    bool usedRunningStatus = false;
    if ((statusOrData & 0x80u) == 0u) {
      if (runningStatus == 0) return Fail(errorMessage, "Malformed MIDI track: running status used before a status byte");

      usedRunningStatus = true;
      statusByte = runningStatus;
      firstDataByte = statusOrData;
    } else if (statusByte < 0xF0u)
      runningStatus = statusByte;

    if (statusByte == 0xFFu) {
      uint8_t metaType = 0;
      if (!reader.ReadByte(metaType)) return Fail(errorMessage, "Malformed MIDI track: truncated meta event");

      uint32_t metaLength = 0;
      if (!reader.ReadVariableLengthQuantity(metaLength) || !reader.CanRead(metaLength))
        return Fail(errorMessage, "Malformed MIDI track: truncated meta event payload");

      if (metaType == 0x2Fu) {
        if (!reader.Skip(metaLength)) return Fail(errorMessage, "Malformed MIDI track: truncated end-of-track event");
        break;
      }

      if (metaType == 0x51u && metaLength == 3) {
        uint8_t b0 = 0;
        uint8_t b1 = 0;
        uint8_t b2 = 0;
        if (!reader.ReadByte(b0) || !reader.ReadByte(b1) || !reader.ReadByte(b2)) return Fail(errorMessage, "Malformed MIDI track: truncated tempo event");

        const uint32_t microsecondsPerQuarterNote = (static_cast<uint32_t>(b0) << 16) | (static_cast<uint32_t>(b1) << 8) | static_cast<uint32_t>(b2);
        if (microsecondsPerQuarterNote == 0) return Fail(errorMessage, "Malformed MIDI file: tempo event has zero tempo");

        tempoEvents.push_back({absoluteTick, nextSequence++, microsecondsPerQuarterNote});
      } else if (!reader.Skip(metaLength))
        return Fail(errorMessage, "Malformed MIDI track: truncated meta event payload");

      continue;
    }

    if (statusByte == 0xF0u || statusByte == 0xF7u) {
      uint32_t sysexLength = 0;
      if (!reader.ReadVariableLengthQuantity(sysexLength) || !reader.Skip(sysexLength))
        return Fail(errorMessage, "Malformed MIDI track: truncated sysex event");

      continue;
    }

    if (statusByte > 0xEFu) return Fail(errorMessage, "Malformed MIDI track: unsupported status byte");

    const uint8_t statusType = static_cast<uint8_t>(statusByte >> 4);
    const bool hasTwoDataBytes = (statusType == 0x8u) || (statusType == 0x9u) || (statusType == 0xAu) || (statusType == 0xBu) || (statusType == 0xEu);

    uint8_t data1 = 0;
    uint8_t data2 = 0;

    if (usedRunningStatus) data1 = firstDataByte;
    else if (!reader.ReadByte(data1))
      return Fail(errorMessage, "Malformed MIDI track: missing MIDI event data");

    if (hasTwoDataBytes && !reader.ReadByte(data2)) return Fail(errorMessage, "Malformed MIDI track: truncated MIDI event");

    const bool isNoteEvent = statusType == 0x8u || statusType == 0x9u;
    const bool isControlChangeOrPitchBend = statusType == 0xBu || statusType == 0xEu;
    if (isNoteEvent) trackContainsNoteEvents = true;
    if (isNoteEvent || isControlChangeOrPitchBend) rawEvents.push_back({absoluteTick, nextSequence++, iplug::IMidiMsg{0, statusByte, data1, data2}});
  }

  trackEndTick = absoluteTick;
  return reader.Position() <= trackEndPosition;
}

bool NormalizeMonophonic(std::vector<RawEvent>& events, uint64_t trackEndTick, std::string* errorMessage) {
  bool sawNoteOn = false;
  std::optional<ActiveNote> activeNote;
  std::vector<RawEvent> normalizedEvents;
  normalizedEvents.reserve(events.size() * 2);
  uint64_t nextSequence = events.empty() ? 0 : (events.back().sequence + 1);

  for (const RawEvent& event : events) {
    if (IsNoteOffMessage(event.message)) {
      if (activeNote && activeNote->channel == event.message.Channel() && activeNote->note == event.message.NoteNumber()) {
        normalizedEvents.push_back(event);
        activeNote.reset();
      }
      continue;
    }

    if (!IsNoteOnMessage(event.message)) {
      normalizedEvents.push_back(event);
      continue;
    }

    sawNoteOn = true;

    if (activeNote) normalizedEvents.push_back({event.tick, nextSequence++, MakeNoteOffMessage(activeNote->channel, activeNote->note)});

    normalizedEvents.push_back(event);
    activeNote = ActiveNote{event.message.Channel(), event.message.NoteNumber()};
  }

  if (!sawNoteOn) return Fail(errorMessage, "MIDI file contains no note-on events");

  if (activeNote) normalizedEvents.push_back({trackEndTick, nextSequence++, MakeNoteOffMessage(activeNote->channel, activeNote->note)});

  events = std::move(normalizedEvents);
  return true;
}

// Simultaneous events are ordered so that note-offs come before note-ons, with all other messages before both.
void SortForPlayback(std::vector<RawEvent>& events) {
  std::sort(events.begin(), events.end(), [](const RawEvent& lhs, const RawEvent& rhs) {
    if (lhs.tick != rhs.tick) return lhs.tick < rhs.tick;

    const int lhsPriority = EventPriority(lhs.message);
    const int rhsPriority = EventPriority(rhs.message);
    if (lhsPriority != rhsPriority) return lhsPriority < rhsPriority;

    return lhs.sequence < rhs.sequence;
  });
}

std::vector<TimedEvent> ConvertTicksToSeconds(const std::vector<RawEvent>& events, std::vector<TempoEvent> tempoEvents, uint16_t ticksPerQuarterNote) {
  std::sort(tempoEvents.begin(), tempoEvents.end(),
            [](const TempoEvent& lhs, const TempoEvent& rhs) { return lhs.tick != rhs.tick ? lhs.tick < rhs.tick : lhs.sequence < rhs.sequence; });

  std::vector<TimedEvent> timedEvents;
  timedEvents.reserve(events.size());

  uint64_t currentTick = 0;
  double currentTimeSeconds = 0.0;
  uint32_t currentTempo = kDefaultTempoMicrosecondsPerQuarterNote;
  std::size_t tempoIndex = 0;

  for (const RawEvent& event : events) {
    for (; tempoIndex < tempoEvents.size() && tempoEvents[tempoIndex].tick <= event.tick; ++tempoIndex) {
      const TempoEvent& tempoEvent = tempoEvents[tempoIndex];
      currentTimeSeconds += TicksToSeconds(tempoEvent.tick - currentTick, currentTempo, ticksPerQuarterNote);
      currentTick = tempoEvent.tick;
      currentTempo = tempoEvent.microsecondsPerQuarterNote;
    }

    timedEvents.push_back({currentTimeSeconds + TicksToSeconds(event.tick - currentTick, currentTempo, ticksPerQuarterNote), event.message});
  }

  return timedEvents;
}

bool ParseHeader(ByteReader& reader, uint16_t& trackCount, uint16_t& ticksPerQuarterNote, std::string* errorMessage) {
  char chunkId[5] = {};
  if (!reader.ReadChunkId(chunkId) || std::string_view{chunkId} != "MThd") return Fail(errorMessage, "MIDI file is missing the MThd header chunk");

  uint32_t headerLength = 0;
  uint16_t format = 0;
  uint16_t division = 0;
  if (!reader.ReadUint32BE(headerLength) || !reader.ReadUint16BE(format) || !reader.ReadUint16BE(trackCount) || !reader.ReadUint16BE(division))
    return Fail(errorMessage, "MIDI file header is truncated");

  if (headerLength < 6) return Fail(errorMessage, "MIDI file header chunk is malformed");
  if (!reader.Skip(static_cast<std::size_t>(headerLength - 6))) return Fail(errorMessage, "MIDI file header is truncated");
  if (format > 1) return Fail(errorMessage, "Only MIDI format 0 and format 1 files are supported");
  if ((division & 0x8000u) != 0u) return Fail(errorMessage, "SMPTE-timed MIDI files are not supported");
  if (division == 0) return Fail(errorMessage, "MIDI file has zero ticks per quarter note");

  ticksPerQuarterNote = division;
  return true;
}
} // namespace

bool ParseFile(std::string_view path, ParsedFile& parsedFile, std::string* errorMessage) {
  parsedFile.events.clear();

  std::vector<uint8_t> bytes;
  if (!ReadBinaryFile(path, bytes, errorMessage)) return false;

  ByteReader reader{bytes};
  uint16_t trackCount = 0;
  uint16_t ticksPerQuarterNote = 0;
  if (!ParseHeader(reader, trackCount, ticksPerQuarterNote, errorMessage)) return false;

  std::vector<RawEvent> rawEvents;
  std::vector<TempoEvent> tempoEvents;
  uint64_t nextSequence = 0;
  uint64_t noteTrackEndTick = 0;
  int noteTrackCount = 0;

  for (uint16_t trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
    char chunkId[5] = {};
    uint32_t trackLength = 0;
    if (!reader.ReadChunkId(chunkId)) return Fail(errorMessage, "MIDI file ended before all track chunks were read");
    if (!reader.ReadUint32BE(trackLength)) return Fail(errorMessage, "MIDI track chunk length is truncated");
    if (std::string_view{chunkId} != "MTrk") return Fail(errorMessage, "MIDI file contains a non-track chunk where MTrk was expected");
    if (!reader.CanRead(trackLength)) return Fail(errorMessage, "MIDI track chunk is truncated");

    const std::size_t trackEndPosition = reader.Position() + static_cast<std::size_t>(trackLength);
    bool trackContainsNoteEvents = false;
    uint64_t trackEndTick = 0;
    if (!ParseTrack(reader, trackEndPosition, rawEvents, tempoEvents, nextSequence, trackContainsNoteEvents, trackEndTick, errorMessage)) return false;

    if (trackContainsNoteEvents) {
      ++noteTrackCount;
      noteTrackEndTick = trackEndTick;
    }

    if (!reader.Skip(trackEndPosition - reader.Position())) return Fail(errorMessage, "MIDI track chunk is truncated");
  }

  if (noteTrackCount != 1) return Fail(errorMessage, "MIDI file must contain exactly one track with note events");

  SortForPlayback(rawEvents);
  if (!NormalizeMonophonic(rawEvents, noteTrackEndTick, errorMessage)) return false;

  parsedFile.events = ConvertTicksToSeconds(rawEvents, std::move(tempoEvents), ticksPerQuarterNote);
  return true;
}
} // namespace midi_file

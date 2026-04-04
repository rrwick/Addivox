#include "midi_file_parser.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace midi_file
{
namespace
{
constexpr uint32_t kDefaultTempoMicrosecondsPerQuarterNote = 500000;

struct RawEvent
{
  uint64_t tick{0};
  uint64_t sequence{0};
  iplug::IMidiMsg message{};
};

struct TempoEvent
{
  uint64_t tick{0};
  uint64_t sequence{0};
  uint32_t microsecondsPerQuarterNote{kDefaultTempoMicrosecondsPerQuarterNote};
};

struct ActiveNote
{
  int channel{0};
  int note{0};
  uint64_t startTick{0};
};

class ByteReader
{
public:
  explicit ByteReader(const std::vector<uint8_t>& bytes)
  : mBytes(bytes)
  {
  }

  std::size_t Position() const
  {
    return mPosition;
  }

  std::size_t Remaining() const
  {
    return mBytes.size() - mPosition;
  }

  bool CanRead(std::size_t byteCount) const
  {
    return byteCount <= Remaining();
  }

  bool Skip(std::size_t byteCount)
  {
    if(!CanRead(byteCount))
      return false;

    mPosition += byteCount;
    return true;
  }

  bool ReadByte(uint8_t& value)
  {
    if(!CanRead(1))
      return false;

    value = mBytes[mPosition++];
    return true;
  }

  bool ReadUint16BE(uint16_t& value)
  {
    if(!CanRead(2))
      return false;

    value = static_cast<uint16_t>((static_cast<uint16_t>(mBytes[mPosition]) << 8)
                                  | static_cast<uint16_t>(mBytes[mPosition + 1]));
    mPosition += 2;
    return true;
  }

  bool ReadUint32BE(uint32_t& value)
  {
    if(!CanRead(4))
      return false;

    value = (static_cast<uint32_t>(mBytes[mPosition]) << 24)
      | (static_cast<uint32_t>(mBytes[mPosition + 1]) << 16)
      | (static_cast<uint32_t>(mBytes[mPosition + 2]) << 8)
      | static_cast<uint32_t>(mBytes[mPosition + 3]);
    mPosition += 4;
    return true;
  }

  bool ReadChunkId(char (&chunkId)[5])
  {
    if(!CanRead(4))
      return false;

    for(int index = 0; index < 4; ++index)
      chunkId[index] = static_cast<char>(mBytes[mPosition + static_cast<std::size_t>(index)]);

    chunkId[4] = '\0';
    mPosition += 4;
    return true;
  }

  bool ReadVariableLengthQuantity(uint32_t& value)
  {
    value = 0;

    for(int index = 0; index < 4; ++index)
    {
      uint8_t byte = 0;
      if(!ReadByte(byte))
        return false;

      value = (value << 7) | static_cast<uint32_t>(byte & 0x7Fu);
      if((byte & 0x80u) == 0u)
        return true;
    }

    return false;
  }

private:
  const std::vector<uint8_t>& mBytes;
  std::size_t mPosition{0};
};

void SetErrorMessage(std::string* errorMessage, std::string message)
{
  if(errorMessage)
    *errorMessage = std::move(message);
}

bool ReadBinaryFile(std::string_view path, std::vector<uint8_t>& bytes, std::string* errorMessage)
{
  std::ifstream stream{std::string{path}, std::ios::binary | std::ios::ate};
  if(!stream.is_open())
  {
    SetErrorMessage(errorMessage, "Could not open MIDI file");
    return false;
  }

  const std::ifstream::pos_type endPosition = stream.tellg();
  if(endPosition < 0)
  {
    SetErrorMessage(errorMessage, "Could not determine MIDI file size");
    return false;
  }

  bytes.resize(static_cast<std::size_t>(endPosition));
  stream.seekg(0, std::ios::beg);
  if(!bytes.empty())
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

  if(!stream.good() && !stream.eof())
  {
    SetErrorMessage(errorMessage, "Could not read MIDI file");
    return false;
  }

  return true;
}

bool IsNoteOnMessage(const iplug::IMidiMsg& message)
{
  return message.StatusMsg() == iplug::IMidiMsg::kNoteOn && message.Velocity() > 0;
}

bool IsNoteOffMessage(const iplug::IMidiMsg& message)
{
  if(message.StatusMsg() == iplug::IMidiMsg::kNoteOff)
    return true;

  return message.StatusMsg() == iplug::IMidiMsg::kNoteOn && message.Velocity() == 0;
}

int EventPriority(const iplug::IMidiMsg& message)
{
  if(IsNoteOffMessage(message))
    return 1;

  if(IsNoteOnMessage(message))
    return 2;

  return 0;
}

double TicksToSeconds(uint64_t tickDelta, uint32_t microsecondsPerQuarterNote, uint16_t ticksPerQuarterNote)
{
  return (static_cast<double>(tickDelta) * static_cast<double>(microsecondsPerQuarterNote))
    / (1000000.0 * static_cast<double>(ticksPerQuarterNote));
}

iplug::IMidiMsg MakeNoteOffMessage(int channel, int note)
{
  return iplug::IMidiMsg{
    0,
    static_cast<uint8_t>(0x80 | (channel & 0x0F)),
    static_cast<uint8_t>(note & 0x7F),
    0};
}

bool ParseTrack(ByteReader& reader,
                std::size_t trackEndPosition,
                std::vector<RawEvent>& rawEvents,
                std::vector<TempoEvent>& tempoEvents,
                uint64_t& nextSequence,
                bool& trackContainsNoteEvents,
                uint64_t& trackEndTick,
                std::string* errorMessage)
{
  uint64_t absoluteTick = 0;
  uint8_t runningStatus = 0;
  trackContainsNoteEvents = false;

  while(reader.Position() < trackEndPosition)
  {
    uint32_t deltaTicks = 0;
    if(!reader.ReadVariableLengthQuantity(deltaTicks))
    {
      SetErrorMessage(errorMessage, "Malformed MIDI track: invalid delta time");
      return false;
    }

    absoluteTick += static_cast<uint64_t>(deltaTicks);

    uint8_t statusOrData = 0;
    if(!reader.ReadByte(statusOrData))
    {
      SetErrorMessage(errorMessage, "Malformed MIDI track: unexpected end of track");
      return false;
    }

    uint8_t statusByte = statusOrData;
    uint8_t firstDataByte = 0;
    bool usedRunningStatus = false;
    if((statusOrData & 0x80u) == 0u)
    {
      if(runningStatus == 0)
      {
        SetErrorMessage(errorMessage, "Malformed MIDI track: running status used before a status byte");
        return false;
      }

      usedRunningStatus = true;
      statusByte = runningStatus;
      firstDataByte = statusOrData;
    }
    else if(statusByte < 0xF0u)
      runningStatus = statusByte;

    if(statusByte == 0xFFu)
    {
      uint8_t metaType = 0;
      if(!reader.ReadByte(metaType))
      {
        SetErrorMessage(errorMessage, "Malformed MIDI track: truncated meta event");
        return false;
      }

      uint32_t metaLength = 0;
      if(!reader.ReadVariableLengthQuantity(metaLength) || !reader.CanRead(metaLength))
      {
        SetErrorMessage(errorMessage, "Malformed MIDI track: truncated meta event payload");
        return false;
      }

      if(metaType == 0x2Fu)
      {
        if(!reader.Skip(metaLength))
        {
          SetErrorMessage(errorMessage, "Malformed MIDI track: truncated end-of-track event");
          return false;
        }
        break;
      }

      if(metaType == 0x51u && metaLength == 3)
      {
        uint8_t b0 = 0;
        uint8_t b1 = 0;
        uint8_t b2 = 0;
        if(!reader.ReadByte(b0) || !reader.ReadByte(b1) || !reader.ReadByte(b2))
        {
          SetErrorMessage(errorMessage, "Malformed MIDI track: truncated tempo event");
          return false;
        }

        const uint32_t microsecondsPerQuarterNote =
          (static_cast<uint32_t>(b0) << 16) | (static_cast<uint32_t>(b1) << 8) | static_cast<uint32_t>(b2);
        if(microsecondsPerQuarterNote == 0)
        {
          SetErrorMessage(errorMessage, "Malformed MIDI file: tempo event has zero tempo");
          return false;
        }

        tempoEvents.push_back({absoluteTick, nextSequence++, microsecondsPerQuarterNote});
      }
      else if(!reader.Skip(metaLength))
      {
        SetErrorMessage(errorMessage, "Malformed MIDI track: truncated meta event payload");
        return false;
      }

      continue;
    }

    if(statusByte == 0xF0u || statusByte == 0xF7u)
    {
      uint32_t sysexLength = 0;
      if(!reader.ReadVariableLengthQuantity(sysexLength) || !reader.Skip(sysexLength))
      {
        SetErrorMessage(errorMessage, "Malformed MIDI track: truncated sysex event");
        return false;
      }

      continue;
    }

    if(statusByte < 0x80u || statusByte > 0xEFu)
    {
      SetErrorMessage(errorMessage, "Malformed MIDI track: unsupported status byte");
      return false;
    }

    const uint8_t statusType = static_cast<uint8_t>(statusByte >> 4);
    const bool hasTwoDataBytes =
      (statusType == 0x8u) || (statusType == 0x9u) || (statusType == 0xAu) || (statusType == 0xBu) || (statusType == 0xEu);

    uint8_t data1 = 0;
    uint8_t data2 = 0;

    if(usedRunningStatus)
      data1 = firstDataByte;
    else if(!reader.ReadByte(data1))
    {
      SetErrorMessage(errorMessage, "Malformed MIDI track: missing MIDI event data");
      return false;
    }

    if(hasTwoDataBytes && !reader.ReadByte(data2))
    {
      SetErrorMessage(errorMessage, "Malformed MIDI track: truncated MIDI event");
      return false;
    }

    switch(statusType)
    {
      case 0x8u:
      case 0x9u:
      case 0xBu:
      case 0xEu:
      {
        const iplug::IMidiMsg message{0, statusByte, data1, data2};
        if(statusType == 0x8u || statusType == 0x9u)
          trackContainsNoteEvents = true;
        rawEvents.push_back({absoluteTick, nextSequence++, message});
        break;
      }
      case 0xAu:
      case 0xCu:
      case 0xDu:
      default:
        break;
    }
  }

  trackEndTick = absoluteTick;
  return reader.Position() <= trackEndPosition;
}

bool NormalizeMonophonic(std::vector<RawEvent>& events, uint64_t trackEndTick, std::string* errorMessage)
{
  bool sawNoteOn = false;
  std::optional<ActiveNote> activeNote;
  std::vector<RawEvent> normalizedEvents;
  normalizedEvents.reserve(events.size() * 2);
  uint64_t nextSequence = events.empty() ? 0 : (events.back().sequence + 1);

  for(const RawEvent& event : events)
  {
    if(IsNoteOffMessage(event.message))
    {
      if(activeNote
         && activeNote->channel == event.message.Channel()
         && activeNote->note == event.message.NoteNumber())
      {
        normalizedEvents.push_back(event);
        activeNote.reset();
      }
      continue;
    }

    if(!IsNoteOnMessage(event.message))
    {
      normalizedEvents.push_back(event);
      continue;
    }

    sawNoteOn = true;

    if(activeNote)
    {
      normalizedEvents.push_back(
        {event.tick, nextSequence++, MakeNoteOffMessage(activeNote->channel, activeNote->note)});
    }

    normalizedEvents.push_back(event);
    activeNote = ActiveNote{event.message.Channel(), event.message.NoteNumber(), event.tick};
  }

  if(!sawNoteOn)
  {
    SetErrorMessage(errorMessage, "MIDI file contains no note-on events");
    return false;
  }

  if(activeNote)
    normalizedEvents.push_back(
      {trackEndTick, nextSequence++, MakeNoteOffMessage(activeNote->channel, activeNote->note)});

  events = std::move(normalizedEvents);
  return true;
}

} // namespace

bool ParseFile(std::string_view path, ParsedFile& parsedFile, std::string* errorMessage)
{
  parsedFile.events.clear();

  std::vector<uint8_t> bytes;
  if(!ReadBinaryFile(path, bytes, errorMessage))
    return false;

  ByteReader reader{bytes};

  char chunkId[5] = {};
  if(!reader.ReadChunkId(chunkId) || std::string_view{chunkId} != "MThd")
  {
    SetErrorMessage(errorMessage, "MIDI file is missing the MThd header chunk");
    return false;
  }

  uint32_t headerLength = 0;
  uint16_t format = 0;
  uint16_t trackCount = 0;
  uint16_t division = 0;
  if(!reader.ReadUint32BE(headerLength)
     || !reader.ReadUint16BE(format)
     || !reader.ReadUint16BE(trackCount)
     || !reader.ReadUint16BE(division))
  {
    SetErrorMessage(errorMessage, "MIDI file header is truncated");
    return false;
  }

  if(headerLength < 6)
  {
    SetErrorMessage(errorMessage, "MIDI file header chunk is malformed");
    return false;
  }

  if(!reader.Skip(static_cast<std::size_t>(headerLength - 6)))
  {
    SetErrorMessage(errorMessage, "MIDI file header is truncated");
    return false;
  }

  if(format > 1)
  {
    SetErrorMessage(errorMessage, "Only MIDI format 0 and format 1 files are supported");
    return false;
  }

  if((division & 0x8000u) != 0u)
  {
    SetErrorMessage(errorMessage, "SMPTE-timed MIDI files are not supported");
    return false;
  }

  std::vector<RawEvent> rawEvents;
  std::vector<TempoEvent> tempoEvents;
  uint64_t nextSequence = 0;
  uint64_t noteTrackEndTick = 0;
  int noteTrackCount = 0;

  for(uint16_t trackIndex = 0; trackIndex < trackCount; ++trackIndex)
  {
    if(!reader.ReadChunkId(chunkId))
    {
      SetErrorMessage(errorMessage, "MIDI file ended before all track chunks were read");
      return false;
    }

    uint32_t trackLength = 0;
    if(!reader.ReadUint32BE(trackLength))
    {
      SetErrorMessage(errorMessage, "MIDI track chunk length is truncated");
      return false;
    }

    if(std::string_view{chunkId} != "MTrk")
    {
      SetErrorMessage(errorMessage, "MIDI file contains a non-track chunk where MTrk was expected");
      return false;
    }

    if(!reader.CanRead(trackLength))
    {
      SetErrorMessage(errorMessage, "MIDI track chunk is truncated");
      return false;
    }

    const std::size_t trackEndPosition = reader.Position() + static_cast<std::size_t>(trackLength);
    bool trackContainsNoteEvents = false;
    uint64_t trackEndTick = 0;
    if(!ParseTrack(
         reader,
         trackEndPosition,
         rawEvents,
         tempoEvents,
         nextSequence,
         trackContainsNoteEvents,
         trackEndTick,
         errorMessage))
    {
      return false;
    }

    if(trackContainsNoteEvents)
    {
      ++noteTrackCount;
      noteTrackEndTick = trackEndTick;
    }

    if(!reader.Skip(trackEndPosition - reader.Position()))
    {
      SetErrorMessage(errorMessage, "MIDI track chunk is truncated");
      return false;
    }
  }

  if(noteTrackCount != 1)
  {
    SetErrorMessage(errorMessage, "MIDI file must contain exactly one track with note events");
    return false;
  }

  std::sort(rawEvents.begin(), rawEvents.end(), [](const RawEvent& lhs, const RawEvent& rhs) {
    if(lhs.tick != rhs.tick)
      return lhs.tick < rhs.tick;

    const int lhsPriority = EventPriority(lhs.message);
    const int rhsPriority = EventPriority(rhs.message);
    if(lhsPriority != rhsPriority)
      return lhsPriority < rhsPriority;

    return lhs.sequence < rhs.sequence;
  });

  if(!NormalizeMonophonic(rawEvents, noteTrackEndTick, errorMessage))
    return false;

  std::sort(tempoEvents.begin(), tempoEvents.end(), [](const TempoEvent& lhs, const TempoEvent& rhs) {
    if(lhs.tick != rhs.tick)
      return lhs.tick < rhs.tick;

    return lhs.sequence < rhs.sequence;
  });

  parsedFile.events.reserve(rawEvents.size());

  uint64_t currentTick = 0;
  double currentTimeSeconds = 0.0;
  uint32_t currentTempo = kDefaultTempoMicrosecondsPerQuarterNote;
  std::size_t tempoIndex = 0;

  for(const RawEvent& event : rawEvents)
  {
    while(tempoIndex < tempoEvents.size() && tempoEvents[tempoIndex].tick <= event.tick)
    {
      const TempoEvent& tempoEvent = tempoEvents[tempoIndex];
      currentTimeSeconds += TicksToSeconds(tempoEvent.tick - currentTick, currentTempo, division);
      currentTick = tempoEvent.tick;
      currentTempo = tempoEvent.microsecondsPerQuarterNote;
      ++tempoIndex;
    }

    const double eventTimeSeconds =
      currentTimeSeconds + TicksToSeconds(event.tick - currentTick, currentTempo, division);
    parsedFile.events.push_back({eventTimeSeconds, event.message});
  }

  return true;
}
} // namespace midi_file

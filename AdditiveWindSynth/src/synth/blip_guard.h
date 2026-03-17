#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

class BlipGuard
{
public:
  static constexpr int kMinIntervalSemitones = 2;
  static constexpr int kDefaultIntervalSemitones = 7;
  static constexpr int kMaxIntervalSemitones = 12;
  static constexpr double kMaxDelayMs = 500.0;

  enum class NoteChangeAction
  {
    kOutputNow,
    kWait
  };

  void ResetRuntime()
  {
    mCurrentSample = 0;
    ClearWindow();
  }

  void SetSampleRate(double sampleRate)
  {
    mSampleRate = std::max(sampleRate, 0.0);
  }

  void SetDelayMs(double delayMs)
  {
    mDelayMs = std::clamp(delayMs, 0.0, kMaxDelayMs);
  }

  void SetIntervalSemitones(int intervalSemitones)
  {
    mIntervalSemitones = std::clamp(intervalSemitones, kMinIntervalSemitones, kMaxIntervalSemitones);
  }

  bool HasPending() const
  {
    return mHasPending;
  }

  NoteChangeAction HandleNoteOn(uint8_t currentOutputNote, bool hasCurrentOutput, uint8_t inputNote)
  {
    PruneTrustedHistory();

    if(IsTrustedByHistory(currentOutputNote, hasCurrentOutput, inputNote) || !IsEnabled())
    {
      CancelPending();
      return NoteChangeAction::kOutputNow;
    }

    if(!mHasPending)
      mPendingStartSample = mCurrentSample;

    mHasPending = true;
    mPendingInputNote = inputNote;
    return NoteChangeAction::kWait;
  }

  void Advance(int numFrames)
  {
    mCurrentSample += static_cast<int64_t>(std::max(numFrames, 0));
    PruneTrustedHistory();
  }

  int GetFramesUntilOutputChange() const
  {
    if(!mHasPending)
      return std::numeric_limits<int>::max();

    const int64_t remainingSamples = GetDelaySamples() - GetPendingElapsedSamples();
    if(remainingSamples <= 0)
      return 0;

    return remainingSamples > static_cast<int64_t>(std::numeric_limits<int>::max())
      ? std::numeric_limits<int>::max()
      : static_cast<int>(remainingSamples);
  }

  bool ShouldOutputNow(uint8_t currentOutputNote, bool hasCurrentOutput) const
  {
    if(!mHasPending)
      return false;

    if(!IsEnabled())
      return true;

    if(IsTrustedByHistory(currentOutputNote, hasCurrentOutput, mPendingInputNote))
      return true;

    return GetPendingElapsedSamples() >= GetDelaySamples();
  }

  uint8_t PendingInputNote() const
  {
    return mPendingInputNote;
  }

  void NotifyOutputNote(uint8_t outputNote)
  {
    AppendTrustedNote(outputNote);
    CancelPending();
  }

  void ClearHistory()
  {
    mTrustedHead = 0;
    mTrustedCount = 0;
  }

  void ClearWindow()
  {
    ClearHistory();
    CancelPending();
  }

  void CancelPending()
  {
    mHasPending = false;
    mPendingInputNote = 0;
    mPendingStartSample = 0;
  }

private:
  struct TrustedNote
  {
    uint8_t note{};
    int64_t sample{};
  };

  static constexpr std::size_t kMaxTrustedNotes = 64;

  bool IsEnabled() const
  {
    return GetDelaySamples() > 0;
  }

  bool IsInRange(uint8_t fromNote, uint8_t toNote) const
  {
    return std::abs(static_cast<int>(toNote) - static_cast<int>(fromNote)) < mIntervalSemitones;
  }

  bool IsTrustedByHistory(uint8_t currentOutputNote, bool hasCurrentOutput, uint8_t inputNote) const
  {
    if(hasCurrentOutput && IsInRange(currentOutputNote, inputNote))
      return true;

    for(std::size_t i = 0; i < mTrustedCount; ++i)
    {
      const TrustedNote& trustedNote = GetTrustedNoteFromNewest(i);
      if(IsTrustedNoteInWindow(trustedNote) && IsInRange(trustedNote.note, inputNote))
        return true;
    }

    return false;
  }

  int64_t GetDelaySamples() const
  {
    if(mDelayMs <= 0.0 || mSampleRate <= 0.0)
      return 0;

    return static_cast<int64_t>(std::ceil((mDelayMs * 0.001) * mSampleRate));
  }

  int64_t GetPendingElapsedSamples() const
  {
    return mCurrentSample - mPendingStartSample;
  }

  bool IsTrustedNoteInWindow(const TrustedNote& trustedNote) const
  {
    return (mCurrentSample - trustedNote.sample) <= GetDelaySamples();
  }

  void AppendTrustedNote(uint8_t outputNote)
  {
    const TrustedNote trustedNote{outputNote, mCurrentSample};
    const std::size_t writeIndex = (mTrustedHead + mTrustedCount) % kMaxTrustedNotes;

    mTrustedNotes[writeIndex] = trustedNote;
    if(mTrustedCount < kMaxTrustedNotes)
    {
      ++mTrustedCount;
    }
    else
    {
      mTrustedHead = (mTrustedHead + 1) % kMaxTrustedNotes;
    }
  }

  void PruneTrustedHistory()
  {
    while(mTrustedCount > 0 && !IsTrustedNoteInWindow(mTrustedNotes[mTrustedHead]))
    {
      mTrustedHead = (mTrustedHead + 1) % kMaxTrustedNotes;
      --mTrustedCount;
    }
  }

  const TrustedNote& GetTrustedNoteFromNewest(std::size_t indexFromNewest) const
  {
    const std::size_t newestIndex = (mTrustedHead + mTrustedCount - 1) % kMaxTrustedNotes;
    const std::size_t offset = (newestIndex + kMaxTrustedNotes - (indexFromNewest % kMaxTrustedNotes)) % kMaxTrustedNotes;
    return mTrustedNotes[offset];
  }

  double mSampleRate{0.0};
  double mDelayMs{0.0};
  int mIntervalSemitones{kDefaultIntervalSemitones};
  int64_t mCurrentSample{0};
  bool mHasPending{false};
  uint8_t mPendingInputNote{0};
  int64_t mPendingStartSample{0};
  std::array<TrustedNote, kMaxTrustedNotes> mTrustedNotes{};
  std::size_t mTrustedHead{0};
  std::size_t mTrustedCount{0};
};

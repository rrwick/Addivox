/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
 */

#pragma once

/**
 * @file
 * @copydoc SynthVoice
 */

#include <stdint.h>

#include "IPlugConstants.h"

BEGIN_IPLUG_NAMESPACE

/** A generic synthesizer voice to be controlled by MidiSynth. */
#pragma mark - Voice class

class SynthVoice
{
public:

  virtual ~SynthVoice() {};

  /** @return true if voice is generating any audio. */
  virtual bool GetBusy() const = 0;

  /** Trigger is called when the voice should start, or re-trigger.
   * While the shared control values are sufficient to control a voice, this method can be used to do additional tasks like resetting oscillators.
   * @param level Normalised starting level for this voice, derived from the velocity of the keypress (range 0.0-1.0), or in the case of a re-trigger the existing level
   * @param isRetrigger If this is \c true it means the voice is being re-triggered, and you should accommodate for this in your algorithm */
  virtual void Trigger(double level, bool isRetrigger) {};

  /** Process a block of audio data for the voice
   @param inputs Pointer to input channel arrays. Sometimes synthesisers have audio inputs. Alternatively you can pass in modulation from global LFOs etc here.
   @param outputs Pointer to output channel arrays. Add to the existing data in these arrays.
   @param nInputs The number of input channels that contain valid data
   @param nOutputs The number of output channels that contain valid data
   @param startIdx The start index of the block of samples to process
   @param nFrames The number of samples to process in this block */
  virtual void ProcessSamplesAccumulating(sample** inputs, sample** outputs, int nInputs, int nOutputs, int startIdx, int nFrames)
  {
    for (auto c = 0; c < nOutputs; c++)
    {
      for (auto s = startIdx; s < startIdx + nFrames; s++)
      {
        outputs[c][s] += 0.; // no-op example; a real implementation should accumulate output here
      }
    }
  }

  /** Implement this if you need to do work when the sample rate or block size changes.
   * @param sampleRate The new sample rate
   * @param blockSize The new block size in samples */
  virtual void SetSampleRateAndBlockSize(double sampleRate, int blockSize) {};

protected:
  double mGate{0.};
  double mPitch{0.};
  double mPitchBend{0.};
  int64_t mLastTriggeredTime{-1};
  uint8_t mChannel{0};
  uint8_t mKey{0};
  double mGain{0.}; // used by MidiSynth to hard-kill the voice.

  friend class MidiSynth;
};

END_IPLUG_NAMESPACE

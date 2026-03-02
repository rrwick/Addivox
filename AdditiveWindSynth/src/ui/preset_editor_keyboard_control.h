#pragma once

#include "IControls.h"

#include <algorithm>
#include <array>
#include <initializer_list>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

class PresetEditorKeyboardControl final : public IVKeyboardControl
{
public:
  PresetEditorKeyboardControl(const IRECT& bounds, int minNote = 48, int maxNote = 72)
  : IVKeyboardControl(bounds, minNote, maxNote)
  {
  }

  void SetEditMode(bool editMode)
  {
    if(mEditMode == editMode)
      return;

    mEditMode = editMode;

    if(mEditMode)
    {
      EndMouseNoteIfNeeded();
    }

    SetDirty(false);
  }

  void ClearHighlightedMidiNotes()
  {
    mHighlightedMidiNotes.fill(false);
    SetDirty(false);
  }

  void SetHighlightedMidiNote(int midiNote, bool highlighted)
  {
    if(!IsValidMidiNote(midiNote))
      return;

    const size_t midiNoteIndex = static_cast<size_t>(midiNote);
    if(mHighlightedMidiNotes[midiNoteIndex] == highlighted)
      return;

    mHighlightedMidiNotes[midiNoteIndex] = highlighted;
    SetDirty(false);
  }

  template <size_t N>
  void SetHighlightedMidiNotes(const std::array<int, N>& midiNotes)
  {
    ClearHighlightedMidiNotes();
    for(const int midiNote : midiNotes)
      SetHighlightedMidiNote(midiNote, true);
  }

  void SetHighlightedMidiNotes(std::initializer_list<int> midiNotes)
  {
    ClearHighlightedMidiNotes();
    for(const int midiNote : midiNotes)
      SetHighlightedMidiNote(midiNote, true);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if(mEditMode)
      return;

    IVKeyboardControl::OnMouseDown(x, y, mod);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    if(mEditMode)
      return;

    IVKeyboardControl::OnMouseUp(x, y, mod);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    if(mEditMode)
      return;

    IVKeyboardControl::OnMouseDrag(x, y, dX, dY, mod);
  }

  void OnTouchCancelled(float x, float y, const IMouseMod& mod) override
  {
    if(mEditMode)
      return;

    IVKeyboardControl::OnTouchCancelled(x, y, mod);
  }

  void Draw(IGraphics& g) override
  {
    IVKeyboardControl::Draw(g);

    if(!mEditMode)
      return;

    DrawEditModeHighlights(g);
  }

private:
  static bool IsValidMidiNote(int midiNote)
  {
    return midiNote >= 0 && midiNote <= 127;
  }

  bool IsMidiNoteHighlighted(int midiNote) const
  {
    if(!IsValidMidiNote(midiNote))
      return false;

    return mHighlightedMidiNotes[static_cast<size_t>(midiNote)];
  }

  void EndMouseNoteIfNeeded()
  {
    if(mLastTouchedKey < 0)
      return;

    const int midiNote = GetMidiNoteNumberForKey(mLastTouchedKey);
    if(midiNote >= 0 && GetDelegate())
    {
      IMidiMsg msg;
      msg.MakeNoteOffMsg(midiNote, 0);
      GetDelegate()->SendMidiMsgFromUI(msg);
    }

    SetKeyIsPressed(mLastTouchedKey, false);
    mLastTouchedKey = -1;
    mMouseOverKey = -1;
    mLastVelocity = 0.f;
  }

  void DrawEditModeHighlights(IGraphics& g)
  {
    const int numKeys = mMaxNote - mMinNote + 1;
    if(numKeys <= 0)
      return;

    const float blackKeyBottom = mRECT.T + mRECT.H() * mBKHeightRatio;
    const float blackKeyWidth = (numKeys > 1) ? (mWKWidth * mBKWidthRatio) : mWKWidth;

    for(int key = 0; key < numKeys; ++key)
    {
      const bool isBlack = mIsBlackKeyList.Get()[key];
      if(isBlack)
        continue;

      const int midiNote = mMinNote + key;
      if(!IsMidiNoteHighlighted(midiNote))
        continue;

      const float keyLeft = mKeyXPos.Get()[key];
      const float keyRight = keyLeft + mWKWidth;

      // Bottom portion of white keys is always visible.
      if(blackKeyBottom < mRECT.B)
      {
        const IRECT lowerWhiteBounds{keyLeft, blackKeyBottom, keyRight, mRECT.B};
        g.FillRect(mEditWhiteKeyHighlightColor, lowerWhiteBounds, &mBlend);
      }

      // Top portion avoids the horizontal spans covered by overlapping black keys.
      if(blackKeyBottom > mRECT.T)
      {
        struct Interval
        {
          float l;
          float r;
        };

        std::array<Interval, 4> occludedIntervals{};
        int numOccludedIntervals = 0;

        for(int blackKey = 0; blackKey < numKeys; ++blackKey)
        {
          if(!mIsBlackKeyList.Get()[blackKey])
            continue;

          const float blackLeft = mKeyXPos.Get()[blackKey];
          const float blackRight = blackLeft + blackKeyWidth;
          const float overlapLeft = std::max(keyLeft, blackLeft);
          const float overlapRight = std::min(keyRight, blackRight);
          if(overlapRight <= overlapLeft)
            continue;

          if(numOccludedIntervals < static_cast<int>(occludedIntervals.size()))
            occludedIntervals[static_cast<size_t>(numOccludedIntervals++)] = {overlapLeft, overlapRight};
        }

        if(numOccludedIntervals == 0)
        {
          const IRECT upperWhiteBounds{keyLeft, mRECT.T, keyRight, blackKeyBottom};
          g.FillRect(mEditWhiteKeyHighlightColor, upperWhiteBounds, &mBlend);
        }
        else
        {
          std::sort(occludedIntervals.begin(),
                    occludedIntervals.begin() + numOccludedIntervals,
                    [](const Interval& a, const Interval& b) { return a.l < b.l; });

          float visibleStart = keyLeft;
          for(int intervalIndex = 0; intervalIndex < numOccludedIntervals; ++intervalIndex)
          {
            const Interval& occluded = occludedIntervals[static_cast<size_t>(intervalIndex)];

            if(occluded.l > visibleStart)
            {
              const IRECT visibleUpperWhiteBounds{visibleStart, mRECT.T, occluded.l, blackKeyBottom};
              g.FillRect(mEditWhiteKeyHighlightColor, visibleUpperWhiteBounds, &mBlend);
            }

            visibleStart = std::max(visibleStart, occluded.r);
          }

          if(visibleStart < keyRight)
          {
            const IRECT trailingUpperWhiteBounds{visibleStart, mRECT.T, keyRight, blackKeyBottom};
            g.FillRect(mEditWhiteKeyHighlightColor, trailingUpperWhiteBounds, &mBlend);
          }
        }
      }
    }

    for(int key = 0; key < numKeys; ++key)
    {
      const bool isBlack = mIsBlackKeyList.Get()[key];
      if(!isBlack)
        continue;

      const int midiNote = mMinNote + key;
      if(!IsMidiNoteHighlighted(midiNote))
        continue;

      const float keyLeft = mKeyXPos.Get()[key];
      const IRECT keyBounds{keyLeft, mRECT.T, keyLeft + blackKeyWidth, blackKeyBottom};
      g.FillRect(mEditBlackKeyHighlightColor, keyBounds, &mBlend);
    }
  }

  bool mEditMode{false};
  std::array<bool, 128> mHighlightedMidiNotes{};
  IColor mEditWhiteKeyHighlightColor{100, 66, 130, 255};
  IColor mEditBlackKeyHighlightColor{155, 48, 110, 255};
};
} // namespace plugin_ui

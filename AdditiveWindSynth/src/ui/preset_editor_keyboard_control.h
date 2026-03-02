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
    ReplaceHighlightedMidiNotes(midiNotes);
  }

  void SetHighlightedMidiNotes(std::initializer_list<int> midiNotes)
  {
    ReplaceHighlightedMidiNotes(midiNotes);
  }

  void SetSelectedMidiNote(int midiNote)
  {
    const int selectedMidiNote = IsMidiNoteInRange(midiNote) ? midiNote : -1;
    if(mSelectedMidiNote == selectedMidiNote)
      return;

    mSelectedMidiNote = selectedMidiNote;
    SetDirty(false);
  }

  int GetSelectedMidiNote() const
  {
    return mSelectedMidiNote;
  }

  void ClearSelectedMidiNote()
  {
    SetSelectedMidiNote(-1);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if(mEditMode)
    {
      SelectNoteAtPoint(x, y);
      return;
    }

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
    {
      SelectNoteAtPoint(x, y);
      return;
    }

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
  static constexpr int kMaxUpperOcclusions = 4;

  struct KeyGeometry
  {
    int numKeys = 0;
    float blackKeyBottom = 0.f;
    float blackKeyWidth = 0.f;
  };

  struct KeyNoteDotGeometry
  {
    float whiteRegionTop = 0.f;
    float radius = 0.f;
    float bottomOffset = 0.f;
  };

  struct Interval
  {
    float l = 0.f;
    float r = 0.f;
  };

  template <typename TMidiNotes>
  void ReplaceHighlightedMidiNotes(const TMidiNotes& midiNotes)
  {
    ClearHighlightedMidiNotes();
    for(const int midiNote : midiNotes)
      SetHighlightedMidiNote(midiNote, true);
  }

  static bool IsValidMidiNote(int midiNote)
  {
    return midiNote >= 0 && midiNote <= 127;
  }

  KeyGeometry GetKeyGeometry() const
  {
    const int numKeys = mMaxNote - mMinNote + 1;
    if(numKeys <= 0)
      return {};

    return {numKeys, mRECT.T + mRECT.H() * mBKHeightRatio, (numKeys > 1) ? (mWKWidth * mBKWidthRatio) : mWKWidth};
  }

  KeyNoteDotGeometry GetKeyNoteDotGeometry(const KeyGeometry& geometry) const
  {
    const float whiteRegionTop = std::clamp(geometry.blackKeyBottom, mRECT.T, mRECT.B);
    const float whiteRegionHeight = std::max(1.f, mRECT.B - whiteRegionTop);
    const float radius = std::min(mWKWidth, whiteRegionHeight) * mWhiteKeyDotRadiusFactor;
    return {whiteRegionTop, radius, whiteRegionHeight * 0.5f};
  }

  bool IsMidiNoteInRange(int midiNote) const
  {
    return midiNote >= mMinNote && midiNote <= mMaxNote;
  }

  bool IsMidiNoteHighlighted(int midiNote) const
  {
    if(!IsValidMidiNote(midiNote))
      return false;

    return mHighlightedMidiNotes[static_cast<size_t>(midiNote)];
  }

  bool IsMidiNoteSelected(int midiNote) const
  {
    return midiNote == mSelectedMidiNote;
  }

  void SelectNoteAtPoint(float x, float y)
  {
    const int keyIndex = GetKeyAtPointForEditMode(x, y);
    if(keyIndex >= 0)
      SetSelectedMidiNote(GetMidiNoteNumberForKey(keyIndex));
  }

  int GetKeyAtPointForEditMode(float x, float y) const
  {
    IRECT clipRect = mRECT.GetPadded(-2.f);
    clipRect.Constrain(x, y);

    const KeyGeometry geometry = GetKeyGeometry();
    if(geometry.numKeys <= 0)
      return -1;

    for(int key = 0; key < geometry.numKeys; ++key)
    {
      if(!mIsBlackKeyList.Get()[key])
        continue;

      const float keyLeft = mKeyXPos.Get()[key];
      const IRECT keyBounds{keyLeft, mRECT.T, keyLeft + geometry.blackKeyWidth, geometry.blackKeyBottom};
      if(keyBounds.Contains(x, y))
        return key;
    }

    for(int key = 0; key < geometry.numKeys; ++key)
    {
      if(mIsBlackKeyList.Get()[key])
        continue;

      const float keyLeft = mKeyXPos.Get()[key];
      const IRECT keyBounds{keyLeft, mRECT.T, keyLeft + mWKWidth, mRECT.B};
      if(keyBounds.Contains(x, y))
        return key;
    }

    return -1;
  }

  int CollectUpperOcclusions(float keyLeft,
                             float keyRight,
                             const KeyGeometry& geometry,
                             std::array<Interval, kMaxUpperOcclusions>& occlusions) const
  {
    int numOcclusions = 0;
    for(int key = 0; key < geometry.numKeys; ++key)
    {
      if(!mIsBlackKeyList.Get()[key])
        continue;

      const float blackLeft = mKeyXPos.Get()[key];
      const float blackRight = blackLeft + geometry.blackKeyWidth;
      const float overlapLeft = std::max(keyLeft, blackLeft);
      const float overlapRight = std::min(keyRight, blackRight);
      if(overlapRight <= overlapLeft)
        continue;

      if(numOcclusions < static_cast<int>(occlusions.size()))
        occlusions[static_cast<size_t>(numOcclusions++)] = {overlapLeft, overlapRight};
    }

    return numOcclusions;
  }

  void DrawSelectedWhiteKeyFill(IGraphics& g, int key, const KeyGeometry& geometry, const IColor& fillColor)
  {
    const float keyLeft = mKeyXPos.Get()[key];
    const float keyRight = keyLeft + mWKWidth;

    if(geometry.blackKeyBottom < mRECT.B)
      g.FillRect(fillColor, {keyLeft, geometry.blackKeyBottom, keyRight, mRECT.B}, &mBlend);

    if(geometry.blackKeyBottom <= mRECT.T)
      return;

    std::array<Interval, kMaxUpperOcclusions> occlusions{};
    const int numOcclusions = CollectUpperOcclusions(keyLeft, keyRight, geometry, occlusions);
    if(numOcclusions == 0)
    {
      g.FillRect(fillColor, {keyLeft, mRECT.T, keyRight, geometry.blackKeyBottom}, &mBlend);
      return;
    }

    std::sort(occlusions.begin(),
              occlusions.begin() + numOcclusions,
              [](const Interval& a, const Interval& b) { return a.l < b.l; });

    float visibleStart = keyLeft;
    for(int i = 0; i < numOcclusions; ++i)
    {
      const Interval& occlusion = occlusions[static_cast<size_t>(i)];
      if(occlusion.l > visibleStart)
        g.FillRect(fillColor, {visibleStart, mRECT.T, occlusion.l, geometry.blackKeyBottom}, &mBlend);

      visibleStart = std::max(visibleStart, occlusion.r);
    }

    if(visibleStart < keyRight)
      g.FillRect(fillColor, {visibleStart, mRECT.T, keyRight, geometry.blackKeyBottom}, &mBlend);
  }

  void DrawWhiteKeyNoteDot(IGraphics& g, int key, const KeyNoteDotGeometry& dotGeometry)
  {
    const float keyMidX = mKeyXPos.Get()[key] + (mWKWidth * 0.5f);
    const float dotCenterY = std::clamp(mRECT.B - dotGeometry.bottomOffset,
                                        dotGeometry.whiteRegionTop + dotGeometry.radius,
                                        mRECT.B - dotGeometry.radius);
    g.FillCircle(mKeyNoteDotColor, keyMidX, dotCenterY, dotGeometry.radius, &mBlend);
  }

  void DrawBlackKeyNoteDot(IGraphics& g, const IRECT& keyBounds, const KeyNoteDotGeometry& dotGeometry)
  {
    const float dotCenterY = std::clamp(keyBounds.B - dotGeometry.bottomOffset,
                                        keyBounds.T + dotGeometry.radius,
                                        keyBounds.B - dotGeometry.radius);
    g.FillCircle(mKeyNoteDotColor, keyBounds.MW(), dotCenterY, dotGeometry.radius, &mBlend);
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
    const KeyGeometry geometry = GetKeyGeometry();
    if(geometry.numKeys <= 0)
      return;

    const IColor selectedWhiteFillColor = mPK_COLOR;
    IColor selectedBlackFillColor = mPK_COLOR;
    selectedBlackFillColor.A = static_cast<int>(mBKAlpha);
    const KeyNoteDotGeometry dotGeometry = GetKeyNoteDotGeometry(geometry);

    for(int key = 0; key < geometry.numKeys; ++key)
    {
      if(mIsBlackKeyList.Get()[key])
        continue;

      const int midiNote = mMinNote + key;
      const bool isSelected = IsMidiNoteSelected(midiNote);
      const bool isKeyNote = IsMidiNoteHighlighted(midiNote);
      if(!isSelected && !isKeyNote)
        continue;

      if(isSelected)
        DrawSelectedWhiteKeyFill(g, key, geometry, selectedWhiteFillColor);

      if(isKeyNote)
        DrawWhiteKeyNoteDot(g, key, dotGeometry);
    }

    for(int key = 0; key < geometry.numKeys; ++key)
    {
      if(!mIsBlackKeyList.Get()[key])
        continue;

      const int midiNote = mMinNote + key;
      const bool isSelected = IsMidiNoteSelected(midiNote);
      const bool isKeyNote = IsMidiNoteHighlighted(midiNote);
      if(!isSelected && !isKeyNote)
        continue;

      const float keyLeft = mKeyXPos.Get()[key];
      const IRECT keyBounds{keyLeft, mRECT.T, keyLeft + geometry.blackKeyWidth, geometry.blackKeyBottom};
      if(isSelected)
        g.FillRect(selectedBlackFillColor, keyBounds, &mBlend);

      if(isKeyNote)
        DrawBlackKeyNoteDot(g, keyBounds, dotGeometry);
    }
  }

  bool mEditMode{false};
  int mSelectedMidiNote{-1};
  std::array<bool, 128> mHighlightedMidiNotes{};
  IColor mKeyNoteDotColor{255, 32, 130, 245};
  float mWhiteKeyDotRadiusFactor{0.4f};
};
} // namespace plugin_ui

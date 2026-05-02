#pragma once

#include "IControls.h"

#include <functional>
#include <initializer_list>
#include <utility>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

class ActionSelectionControl final : public ICaptionControl {
public:
  using SelectionFunc = std::function<void(const char* selectedText)>;

  ActionSelectionControl(const IRECT& bounds, const char* defaultText, const std::initializer_list<const char*>& menuItems, const IText& text,
                         const IColor& bgColor = COLOR_TRANSPARENT, bool persistentSelection = false)
      : ICaptionControl(bounds, kNoParameter, text, bgColor, false), mMenu("", menuItems), mPersistentSelection(persistentSelection) {
    if (defaultText && defaultText[0] != '\0') mDefaultText.Set(defaultText);
    else if (defaultText == nullptr && mMenu.NItems() > 0)
      mDefaultText.Set(mMenu.GetItem(0)->GetText());

    SetStr(mDefaultText.Get());
    DisablePrompt(true);
  }

  void SetOnSelection(SelectionFunc func) { mOnSelection = std::move(func); }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override {
    if (IsDisabled() || !(mod.L || mod.R) || !GetUI()) return;

    if (!mPersistentSelection) mMenu.SetChosenItemIdx(-1);

    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override {
    const char* selectedText = nullptr;
    if (pSelectedMenu && pSelectedMenu->GetChosenItem()) selectedText = pSelectedMenu->GetChosenItem()->GetText();

    if (selectedText && mOnSelection) mOnSelection(selectedText);

    if (selectedText && mPersistentSelection) SetStr(selectedText);
    else if (!mPersistentSelection)
      SetStr(mDefaultText.Get());

    if (!mPersistentSelection) mMenu.SetChosenItemIdx(-1);

    SetDirty(false);
    IControl::OnPopupMenuSelection(pSelectedMenu, valIdx);
  }

  void OnResize() override {
    const float dropDownAreaWidth = mText.mSize;
    const float triangleWidth = dropDownAreaWidth * 0.5f;
    const float triangleHeight = dropDownAreaWidth * 0.33f;

    auto dropDownAreaRect = mText.mAlign == EAlign::Far ? mRECT.GetFromLeft(dropDownAreaWidth) : mRECT.GetFromRight(dropDownAreaWidth);
    mTriangleRect = dropDownAreaRect.GetCentredInside(IRECT(0.f, 0.f, triangleWidth, triangleHeight));
  }

private:
  WDL_String mDefaultText{};
  IPopupMenu mMenu;
  bool mPersistentSelection{false};
  SelectionFunc mOnSelection{};
};

} // namespace plugin_ui

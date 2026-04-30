#pragma once

#include "IControls.h"
#include "IVPresetManagerControls.h"
#include "colour.h"
#include "editor_messages.h"
#include "help_text.h"

#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace layout
{
namespace detail
{
inline const char* DefaultShowInFileBrowserLabel()
{
#if defined(OS_WIN)
  return "Show User Patches in Explorer";
#else
  return "Show User Patches in Finder";
#endif
}

template <typename Callback>
inline IActionFunction MakeImmediatePatchButtonAction(Callback&& callback)
{
  return [cb = std::forward<Callback>(callback)](IControl* caller) mutable {
    if(caller)
    {
      caller->SetValue(0.);
      caller->SetDirty(false);
    }

    cb(caller);
  };
}
} // namespace detail

struct PatchMenuEntry
{
  int id{-1};
  std::string name;
  std::vector<std::string> groupPath;
  bool checked{false};
};

struct PatchMenuModel
{
  std::vector<PatchMenuEntry> entries;
  std::string label{"Choose Patch..."};
  std::string showInFileBrowserLabel{detail::DefaultShowInFileBrowserLabel()};
  bool canShowInFileBrowser{false};
};

class PatchManagerControl final : public IVBakedPresetManagerControl
{
public:
  using IVBakedPresetManagerControl::IVBakedPresetManagerControl;

  void SetPatchMenuModel(PatchMenuModel model)
  {
    mModel = std::move(model);
    SetPatchLabel(mModel.label.c_str());
  }

  void SetPatchLabel(const char* label)
  {
    if(!mPatchNameButton)
      return;

    mPatchNameButton->SetLabelStr((label && label[0] != '\0') ? label : "Choose Patch...");
  }

  void OnAttached() override
  {
    const auto prevPatchFunc = [this](IControl*) {
      SendPatchManagerAction(editor_messages::PatchManagerAction::PreviousPatch);
    };

    const auto nextPatchFunc = [this](IControl*) {
      SendPatchManagerAction(editor_messages::PatchManagerAction::NextPatch);
    };

    const auto choosePatchFunc = [this](IControl* caller) {
      BuildPatchMenu();

      caller->GetUI()->CreatePopupMenu(*this, mPatchMenu, caller->GetRECT());
    };

    const IVStyle patchNameStyle = mStyle.WithLabelText(mStyle.labelText.WithSize(18.f));
    SetTooltip(help_text::main_ui::kPatchManager);

    auto* previousPatchButton = new PatchArrowButtonControl(
      IRECT(),
      detail::MakeImmediatePatchButtonAction(prevPatchFunc),
      PatchArrowButtonControl::Direction::Left,
      mStyle);
    previousPatchButton->SetTooltip(help_text::main_ui::kPreviousPatch);
    AddChildControl(previousPatchButton);

    auto* nextPatchButton = new PatchArrowButtonControl(
      IRECT(),
      detail::MakeImmediatePatchButtonAction(nextPatchFunc),
      PatchArrowButtonControl::Direction::Right,
      mStyle);
    nextPatchButton->SetTooltip(help_text::main_ui::kNextPatch);
    AddChildControl(nextPatchButton);

    AddChildControl(mPatchNameButton = new IVButtonControl(
      IRECT(),
      detail::MakeImmediatePatchButtonAction(choosePatchFunc),
      "Choose Patch...",
      patchNameStyle));
    mPatchNameButton->SetTooltip(help_text::main_ui::kPatchManager);

    OnResize();
    SetPatchLabel(mModel.label.c_str());
  }

  void OnPopupMenuSelection(IPopupMenu* selectedMenu, int valIdx) override
  {
    if(!selectedMenu)
      return;

    auto* selectedItem = selectedMenu->GetChosenItem();
    if(!selectedItem)
      return;

    const int tag = selectedItem->GetTag();
    if(tag < 0 || tag >= static_cast<int>(mActionItems.size()))
      return;

    const ActionItem& actionItem = mActionItems[static_cast<std::size_t>(tag)];
    SendPatchManagerAction(actionItem.action, actionItem.patchId);
  }

  void OnResize() override
  {
    MakeRects(mRECT);

    ForAllChildrenFunc([&](int childIdx, IControl* child) {
      child->SetTargetAndDrawRECTs(GetSubControlBounds(static_cast<ESubControl>(childIdx)));
    });
  }

private:
  class PatchArrowButtonControl final : public IVButtonControl
  {
  public:
    enum class Direction
    {
      Left,
      Right
    };

    PatchArrowButtonControl(const IRECT& bounds, IActionFunction actionFunc, Direction direction, const IVStyle& style)
    : IVButtonControl(bounds, std::move(actionFunc), "", style, false, false)
    , mDirection(direction)
    {
    }

    void DrawWidget(IGraphics& g) override
    {
      IVButtonControl::DrawWidget(g);

      const IRECT arrowBounds = mWidgetBounds.GetPadded(-9.f);
      const IColor arrowColor = IsDisabled() ? GetColor(kSH) : colour::ui::kValueText;
      const float sideLength = std::min(arrowBounds.W(), arrowBounds.H() / 0.8660254f);
      const float triangleHeight = sideLength * 0.8660254f;
      const float left = arrowBounds.MW() - triangleHeight * 0.5f;
      const float right = arrowBounds.MW() + triangleHeight * 0.5f;
      const float top = arrowBounds.MH() - sideLength * 0.5f;
      const float bottom = arrowBounds.MH() + sideLength * 0.5f;
      const float midY = arrowBounds.MH();

      if(mDirection == Direction::Left)
      {
        g.FillTriangle(
          arrowColor,
          left,
          midY,
          right,
          top,
          right,
          bottom);
      }
      else
      {
        g.FillTriangle(
          arrowColor,
          left,
          top,
          right,
          midY,
          left,
          bottom);
      }
    }

  private:
    Direction mDirection;
  };

  struct ActionItem
  {
    editor_messages::PatchManagerAction action{editor_messages::PatchManagerAction::SelectPatch};
    int patchId{-1};
  };

  int AddActionItem(editor_messages::PatchManagerAction action, int patchId = -1)
  {
    const int tag = static_cast<int>(mActionItems.size());
    mActionItems.push_back(ActionItem{action, patchId});
    return tag;
  }

  void SendPatchManagerAction(editor_messages::PatchManagerAction action, int patchId = -1)
  {
    auto* delegate = GetDelegate();
    if(!delegate)
      return;

    const editor_messages::PatchManagerActionPayload payload{
      static_cast<int>(action),
      patchId
    };
    delegate->SendArbitraryMsgFromUI(
      editor_messages::kMsgTagPatchManagerAction,
      GetTag(),
      sizeof(payload),
      &payload);
  }

  IPopupMenu* EnsureSubmenu(IPopupMenu& parent, const std::string& name)
  {
    for(int i = 0; i < parent.NItems(); ++i)
    {
      IPopupMenu::Item* item = parent.GetItem(i);
      if(item && item->GetSubmenu() && std::strcmp(item->GetText(), name.c_str()) == 0)
        return item->GetSubmenu();
    }

    return parent.AddItem(name.c_str(), new IPopupMenu(name.c_str()))->GetSubmenu();
  }

  static std::string GetDisplayGroupName(const std::string& group)
  {
    if(group == "Factory")
      return "Factory Patches";
    if(group == "User")
      return "User Patches";
    return group;
  }

  void AddPatchEntryToMenu(const PatchMenuEntry& entry)
  {
    IPopupMenu* menu = &mPatchMenu;
    for(const auto& group : entry.groupPath)
      menu = EnsureSubmenu(*menu, GetDisplayGroupName(group));

    const int flags = entry.checked ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags;
    menu->AddItem(
      new IPopupMenu::Item(
        entry.name.c_str(),
        flags,
        AddActionItem(editor_messages::PatchManagerAction::SelectPatch, entry.id)));
  }

  void AddCommandItem(const char* label,
                      editor_messages::PatchManagerAction action,
                      bool enabled = true)
  {
    const int flags = enabled ? IPopupMenu::Item::kNoFlags : IPopupMenu::Item::kDisabled;
    mPatchMenu.AddItem(new IPopupMenu::Item(label, flags, AddActionItem(action)));
  }

  void BuildPatchMenu()
  {
    mPatchMenu.Clear();
    mActionItems.clear();

    if(mModel.entries.empty())
    {
      mPatchMenu.AddItem("(No patches found)", -1, IPopupMenu::Item::kDisabled);
    }
    else
    {
      for(const auto& entry : mModel.entries)
        AddPatchEntryToMenu(entry);
    }

    mPatchMenu.AddSeparator();
    AddCommandItem("Save...", editor_messages::PatchManagerAction::SavePatch);
    AddCommandItem("Import Patch...", editor_messages::PatchManagerAction::ImportPatch);
    AddCommandItem("Import Collection...", editor_messages::PatchManagerAction::ImportCollection);
    AddCommandItem(
      mModel.showInFileBrowserLabel.empty() ? detail::DefaultShowInFileBrowserLabel() : mModel.showInFileBrowserLabel.c_str(),
      editor_messages::PatchManagerAction::ShowPatchInFileBrowser,
      mModel.canShowInFileBrowser);
    AddCommandItem("Refresh", editor_messages::PatchManagerAction::RefreshPatches);
  }

  IRECT GetSubControlBounds(ESubControl control) const
  {
    auto sections = mWidgetBounds;

    std::array<IRECT, 3> rects = {
      sections.ReduceFromLeft(30),
      sections.ReduceFromLeft(30),
      sections
    };

    return rects[static_cast<int>(control)];
  }

  IPopupMenu mPatchMenu;
  PatchMenuModel mModel;
  std::vector<ActionItem> mActionItems;
  IVButtonControl* mPatchNameButton = nullptr;
};
} // namespace layout
} // namespace plugin_ui

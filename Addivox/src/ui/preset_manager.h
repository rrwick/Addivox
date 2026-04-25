#pragma once

#include "IControls.h"
#include "IVPresetManagerControls.h"
#include "colour.h"
#include "editor_messages.h"

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
template <typename Callback>
inline IActionFunction MakeImmediatePresetButtonAction(Callback&& callback)
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

struct PresetMenuEntry
{
  int id{-1};
  std::string name;
  std::vector<std::string> groupPath;
  bool checked{false};
};

struct PresetMenuModel
{
  std::vector<PresetMenuEntry> entries;
  std::string label{"Choose Preset..."};
  std::string showInFileBrowserLabel{"Show in Finder"};
  bool canShowInFileBrowser{false};
};

class PresetManagerControl final : public IVBakedPresetManagerControl
{
public:
  using IVBakedPresetManagerControl::IVBakedPresetManagerControl;

  void SetPresetMenuModel(PresetMenuModel model)
  {
    mModel = std::move(model);
    SetPresetLabel(mModel.label.c_str());
  }

  void SetPresetLabel(const char* label)
  {
    if(!mPresetNameButton)
      return;

    mPresetNameButton->SetLabelStr((label && label[0] != '\0') ? label : "Choose Preset...");
  }

  void OnAttached() override
  {
    const auto prevPresetFunc = [this](IControl*) {
      SendPresetManagerAction(editor_messages::PresetManagerAction::PreviousPreset);
    };

    const auto nextPresetFunc = [this](IControl*) {
      SendPresetManagerAction(editor_messages::PresetManagerAction::NextPreset);
    };

    const auto choosePresetFunc = [this](IControl* caller) {
      BuildPresetMenu();

      caller->GetUI()->CreatePopupMenu(*this, mPresetMenu, caller->GetRECT());
    };

    const IVStyle presetNameStyle = mStyle.WithLabelText(mStyle.labelText.WithSize(18.f));
    AddChildControl(new PresetArrowButtonControl(
      IRECT(),
      detail::MakeImmediatePresetButtonAction(prevPresetFunc),
      PresetArrowButtonControl::Direction::Left,
      mStyle));
    AddChildControl(new PresetArrowButtonControl(
      IRECT(),
      detail::MakeImmediatePresetButtonAction(nextPresetFunc),
      PresetArrowButtonControl::Direction::Right,
      mStyle));
    AddChildControl(mPresetNameButton = new IVButtonControl(
      IRECT(),
      detail::MakeImmediatePresetButtonAction(choosePresetFunc),
      "Choose Preset...",
      presetNameStyle));

    OnResize();
    SetPresetLabel(mModel.label.c_str());
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
    SendPresetManagerAction(actionItem.action, actionItem.presetId);
  }

  void OnResize() override
  {
    MakeRects(mRECT);

    ForAllChildrenFunc([&](int childIdx, IControl* child) {
      child->SetTargetAndDrawRECTs(GetSubControlBounds(static_cast<ESubControl>(childIdx)));
    });
  }

private:
  class PresetArrowButtonControl final : public IVButtonControl
  {
  public:
    enum class Direction
    {
      Left,
      Right
    };

    PresetArrowButtonControl(const IRECT& bounds, IActionFunction actionFunc, Direction direction, const IVStyle& style)
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
    editor_messages::PresetManagerAction action{editor_messages::PresetManagerAction::SelectPreset};
    int presetId{-1};
  };

  int AddActionItem(editor_messages::PresetManagerAction action, int presetId = -1)
  {
    const int tag = static_cast<int>(mActionItems.size());
    mActionItems.push_back(ActionItem{action, presetId});
    return tag;
  }

  void SendPresetManagerAction(editor_messages::PresetManagerAction action, int presetId = -1)
  {
    auto* delegate = GetDelegate();
    if(!delegate)
      return;

    const editor_messages::PresetManagerActionPayload payload{
      static_cast<int>(action),
      presetId
    };
    delegate->SendArbitraryMsgFromUI(
      editor_messages::kMsgTagPresetManagerAction,
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

  void AddPresetEntryToMenu(const PresetMenuEntry& entry)
  {
    IPopupMenu* menu = &mPresetMenu;
    for(const auto& group : entry.groupPath)
      menu = EnsureSubmenu(*menu, group);

    const int flags = entry.checked ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags;
    menu->AddItem(
      new IPopupMenu::Item(
        entry.name.c_str(),
        flags,
        AddActionItem(editor_messages::PresetManagerAction::SelectPreset, entry.id)));
  }

  void AddCommandItem(const char* label,
                      editor_messages::PresetManagerAction action,
                      bool enabled = true)
  {
    const int flags = enabled ? IPopupMenu::Item::kNoFlags : IPopupMenu::Item::kDisabled;
    mPresetMenu.AddItem(new IPopupMenu::Item(label, flags, AddActionItem(action)));
  }

  void BuildPresetMenu()
  {
    mPresetMenu.Clear();
    mActionItems.clear();

    if(mModel.entries.empty())
    {
      mPresetMenu.AddItem("(No presets found)", -1, IPopupMenu::Item::kDisabled);
    }
    else
    {
      for(const auto& entry : mModel.entries)
        AddPresetEntryToMenu(entry);
    }

    mPresetMenu.AddSeparator();
    AddCommandItem("Save...", editor_messages::PresetManagerAction::SavePreset);
    AddCommandItem("Import Preset...", editor_messages::PresetManagerAction::ImportPreset);
    AddCommandItem("Import Collection...", editor_messages::PresetManagerAction::ImportCollection);
    AddCommandItem(
      mModel.showInFileBrowserLabel.empty() ? "Show in Finder" : mModel.showInFileBrowserLabel.c_str(),
      editor_messages::PresetManagerAction::ShowPresetInFileBrowser,
      mModel.canShowInFileBrowser);
    AddCommandItem("Refresh", editor_messages::PresetManagerAction::RefreshPresets);
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

  IPopupMenu mPresetMenu;
  PresetMenuModel mModel;
  std::vector<ActionItem> mActionItems;
  IVButtonControl* mPresetNameButton = nullptr;
};
} // namespace layout
} // namespace plugin_ui

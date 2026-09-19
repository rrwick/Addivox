#pragma once

#include "../visualizer/harmonic_visualizer_control.h"
#include "IControls.h"
#include "editor_tabs/attack_release.h"
#include "editor_tabs/breath.h"
#include "editor_tabs/common.h"
#include "editor_tabs/level.h"
#include "editor_tabs/pan.h"
#include "editor_tabs/pitch.h"
#include "editor_tabs/variation.h"
#include "keyboard_control.h"
#include "positions.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

class EditorOscillatorTabPage final : public IVTabPage {
public:
  using VisibilityChangedFunc = std::function<void(bool isVisible)>;
  using ReapplyChildVisibilityFunc = std::function<void()>;

  EditorOscillatorTabPage(TabAttachFunc attachFunc, ResizeFunc resizeFunc, VisibilityChangedFunc visibilityChangedFunc,
                          ReapplyChildVisibilityFunc reapplyChildVisibilityFunc = nullptr)
      : IVTabPage(std::move(attachFunc), std::move(resizeFunc)), mVisibilityChangedFunc(std::move(visibilityChangedFunc)),
        mReapplyChildVisibilityFunc(std::move(reapplyChildVisibilityFunc)) {}

  void Hide(bool hide) override {
    const bool wasHidden = IsHidden();
    IVTabPage::Hide(hide);
    const bool isHidden = IsHidden();

    // Hide(false) unhides every child, even when reselecting the current tab. Restore mode-specific visibility each time.
    if (mReapplyChildVisibilityFunc) mReapplyChildVisibilityFunc();

    if (wasHidden != isHidden && mVisibilityChangedFunc) mVisibilityChangedFunc(!isHidden);
  }

private:
  VisibilityChangedFunc mVisibilityChangedFunc{};
  ReapplyChildVisibilityFunc mReapplyChildVisibilityFunc{};
};

#include "editor_tabs/eq.h"

namespace editor {
class EditorTooltipTabSwitchControl final : public IVTabSwitchControl {
public:
  EditorTooltipTabSwitchControl(const IRECT& bounds, IActionFunction actionFunction, const std::vector<const char*>& options, std::vector<const char*> tooltips,
                                const char* label = "", const IVStyle& style = DEFAULT_STYLE, EVShape shape = EVShape::Rectangle,
                                EDirection direction = EDirection::Horizontal)
      : IVTabSwitchControl(bounds, actionFunction, options, label, style, shape, direction), mTooltips(std::move(tooltips)) {
    UpdateTooltipForButton(-1);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override {
    IVTabSwitchControl::OnMouseOver(x, y, mod);
    UpdateTooltipForButton(mMouseOverButton);
  }

  void OnMouseOut() override {
    IVTabSwitchControl::OnMouseOut();
    UpdateTooltipForButton(-1);
  }

private:
  void UpdateTooltipForButton(int buttonIndex) {
    if (mTooltipButtonIndex == buttonIndex) return;

    mTooltipButtonIndex = buttonIndex;
    const char* tooltip = "";
    if (buttonIndex >= 0 && buttonIndex < static_cast<int>(mTooltips.size()) && mTooltips[static_cast<std::size_t>(buttonIndex)])
      tooltip = mTooltips[static_cast<std::size_t>(buttonIndex)];

    SetTooltip(tooltip);
    if (GetUI() && GetUI()->TooltipsEnabled()) GetUI()->UpdateTooltips();
  }

  std::vector<const char*> mTooltips;
  int mTooltipButtonIndex{-2};
};

class EditorTabbedPagesControl final : public IContainerBase, public IVectorBase {
public:
  EditorTabbedPagesControl(const IRECT& bounds, const PageMap& pages, const char* label = "", const IVStyle& style = DEFAULT_STYLE, float tabBarHeight = 20.0f,
                           float tabBarFrac = 0.5f, EAlign tabsAlign = EAlign::Near)
      : IContainerBase(bounds), IVectorBase(style.WithDrawFrame(false).WithDrawShadows(false)), mTabBarHeight(tabBarHeight), mTabBarFrac(tabBarFrac),
        mTabsAlign(tabsAlign) {
    AttachIControl(this, label);

    for (const auto& page : pages) AddPage(page.first, GetOscillatorTabDescriptionForTitle(page.first), page.second);
  }

  void Hide(bool hide) override {
    if (hide) {
      ForAllChildrenFunc([hide](int childIdx, IControl* child) { child->Hide(hide); });
    } else {
      for (auto* page : mPages) page->Hide(true);

      GetTabSwitchControl()->Hide(false);
      mPages[GetTabSwitchControl()->GetSelectedIdx()]->Hide(false);
    }

    IControl::Hide(hide);
  }

  void Draw(IGraphics& g) override {
    DrawLabel(g);

    const auto cornerRadius = GetRoundedCornerRadius(GetTabBarArea());
    const float tabCornerRadius = mTabBarFrac == 1.0f ? 0.0f : cornerRadius;

    g.FillRoundRect(GetColor(kPR), GetPageArea(), mTabsAlign == EAlign::Near ? 0.0f : tabCornerRadius, mTabsAlign == EAlign::Far ? 0.0f : tabCornerRadius,
                    cornerRadius, cornerRadius);

    if (mStyle.drawFrame) g.DrawRoundRect(GetColor(kFR), mRECT, cornerRadius);
  }

  void OnAttached() override {
    AddChildControl(new EditorTooltipTabSwitchControl(
        GetTabBarArea(), [&](IControl* caller) { ShowSelectedPage(); }, mPageNames, mPageTooltips, "", GetStyle().WithWidgetFrac(1.0f)));

    GetTabSwitchControl()->SetShape(EVShape::EndsRounded);

    for (auto* page : mPages) {
      AddChildControl(page);
      page->SetTargetAndDrawRECTs(GetPageArea());
      page->Hide(true);
    }

    GetTabSwitchControl()->Hide(false);
    mPages.front()->Hide(false);
  }

  void OnStyleChanged() override {
    ForAllChildrenFunc([this](int childIdx, IControl* child) {
      if (auto* vectorBase = child->As<IVectorBase>()) vectorBase->SetStyle(GetStyle());
    });

    const auto adjustedStyle = GetStyle().WithDrawFrame(false).WithDrawShadows(false);
    GetTabSwitchControl()->SetStyle(adjustedStyle.WithWidgetFrac(1.0));
    GetTabSwitchControl()->SetShape(EVShape::EndsRounded);
  }

  void OnResize() override {
    SetTargetRECT(MakeRects(mRECT));

    if (NChildren()) {
      GetTabSwitchControl()->SetTargetAndDrawRECTs(GetTabBarArea());

      for (auto* page : mPages) page->SetTargetAndDrawRECTs(GetPageArea());
    }
  }

  IRECT GetPageArea() const { return mWidgetBounds.GetReducedFromTop(mTabBarHeight); }

  IRECT GetTabBarArea() const { return mWidgetBounds.GetFromTop(mTabBarHeight).FracRectHorizontal(mTabBarFrac, mTabsAlign == EAlign::Far); }

private:
  void AddPage(const char* pageName, const char* pageTooltip, IVTabPage* page) {
    page->SetLabelStr(pageName);
    mPageNames.push_back(pageName);
    mPageTooltips.push_back(pageTooltip ? pageTooltip : "");
    mPages.push_back(page);
  }

  EditorTooltipTabSwitchControl* GetTabSwitchControl() { return GetChild(0)->As<EditorTooltipTabSwitchControl>(); }

  void ShowSelectedPage() {
    const char* selectedTitle = GetTabSwitchControl()->GetSelectedLabelStr();
    for (auto* page : mPages) page->Hide(std::strcmp(selectedTitle, page->GetLabelStr()) != 0);

    if (IBubbleControl* bubbleControl = GetUI()->GetBubbleControl()) bubbleControl->Hide(true);
  }

  std::vector<IVTabPage*> mPages;
  std::vector<const char*> mPageNames;
  std::vector<const char*> mPageTooltips;
  float mTabBarHeight;
  float mTabBarFrac;
  EAlign mTabsAlign;
};

inline const std::vector<OscillatorTabDescriptor>& GetOscillatorTabDescriptors() {
  static const std::vector<OscillatorTabDescriptor> descriptors = [] {
    std::vector<OscillatorTabDescriptor> result;
    result.reserve(OscillatorSettings::kNumParameters);
    AppendLevelTabDescriptors(result);
    AppendBreathTabDescriptors(result);
    AppendAttackReleaseTabDescriptors(result);
    AppendPitchTabDescriptors(result);
    AppendPanTabDescriptors(result);
    AppendVariationTabDescriptors(result);
    return result;
  }();
  return descriptors;
}

inline int GetEditorTabCount() { return static_cast<int>(GetOscillatorTabDescriptors().size()) + 1; }

inline const std::vector<const char*>& GetEditorTabTitlesInDisplayOrder() {
  static const std::vector<const char*> titles = [] {
    std::vector<const char*> result;
    result.reserve(GetEditorTabCount());

    for (const auto& descriptor : GetOscillatorTabDescriptors()) result.push_back(descriptor.title);

    result.push_back(kEqTabTitle);
    std::sort(result.begin(), result.end());
    return result;
  }();
  return titles;
}

inline const OscillatorTabDescriptor* FindOscillatorTabDescriptorForTitle(const char* title) {
  if (!title) return nullptr;

  for (const auto& descriptor : GetOscillatorTabDescriptors()) {
    if (std::strcmp(descriptor.title, title) == 0) return &descriptor;
  }

  return nullptr;
}

inline void ApplyKeyboardActionToSelectedTab(const std::shared_ptr<EditorContext>& context, int keyVK) {
  if (!context || !context->IsEditMode()) return;

  const auto& tabTitles = GetEditorTabTitlesInDisplayOrder();
  const int selectedTabIndex = context->SelectedTabIndex();
  if (selectedTabIndex < 0 || selectedTabIndex >= static_cast<int>(tabTitles.size())) return;

  const char* selectedTitle = tabTitles[static_cast<std::size_t>(selectedTabIndex)];
  if (selectedTitle && std::strcmp(selectedTitle, kEqTabTitle) == 0) {
    const char* actionName = GetEqActionShortcutActionName(keyVK);
    if (!actionName) return;

    auto* editorControl = context->eqTab.editorControl;
    if (!editorControl) return;

    context->ApplyEqCurveActionToSelectedKeyNote(editorControl, [actionName](EqCurve& curve) { return ApplyEqAction(curve, actionName); });
    return;
  }

  const auto* descriptor = FindOscillatorTabDescriptorForTitle(selectedTitle);
  if (!descriptor || context->IsMacrosMode(descriptor->parameter)) return;

  const char* actionName = GetEditorActionShortcutActionName(descriptor->parameter, keyVK);
  if (!actionName) return;

  const auto parameterIndex = static_cast<std::size_t>(descriptor->parameter);
  auto* sliderControl = context->oscillatorTabControls.sliderControls[parameterIndex];
  if (!sliderControl) return;

  const auto parameter = descriptor->parameter;
  const auto editScope = context->GetOscillatorEditScope(parameter);
  const bool applyEditScope = !(parameter == OscillatorParameter::level && MatchesActionLabel(actionName, kActionNormalize));
  context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl, parameter,
      [actionName, editScope, parameter](SimplePatch& patch) {
        switch (parameter) {
        case OscillatorParameter::level:        return ApplyLevelAction(patch, actionName, editScope);
        case OscillatorParameter::breath_power: return ApplyBreathAction(patch, actionName, editScope);
        case OscillatorParameter::attack:
        case OscillatorParameter::release:      return ApplyAttackReleaseAction(patch, parameter, actionName, editScope);
        case OscillatorParameter::pitch:        return ApplyPitchAction(patch, actionName, editScope);
        case OscillatorParameter::pan:          return ApplyPanAction(patch, actionName, editScope);
        default:
          if (IsVariationParameter(parameter)) return ApplyVariationAction(patch, parameter, actionName, editScope);
          return false;
        }
      },
      applyEditScope);
}

inline void AttachOscillatorTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                        const OscillatorTabDescriptor& descriptor) {
  auto* restoreButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Restore", styles.restoreButtonStyle, true, false);
  restoreButton->SetTooltip(help_text::oscillator_tabs::kRestoreButton);
  restoreButton->SetAnimationEndActionFunction([context, descriptor](IControl* caller) { RestoreOscillatorTabValues(context, caller, descriptor); });
  const auto keyNoteActionButtons = CreateKeyNoteActionButtons(context, styles);

  auto* sliderControl = CreateOscillatorSliderControl(context, descriptor, styles);
  const auto parameterIndex = static_cast<std::size_t>(descriptor.parameter);
  context->oscillatorTabControls.sliderControls[parameterIndex] = sliderControl;
  context->oscillatorTabControls.restoreButtons[parameterIndex] = restoreButton;
  context->oscillatorTabControls.addButtons[parameterIndex] = keyNoteActionButtons.addButton;
  context->oscillatorTabControls.deleteButtons[parameterIndex] = keyNoteActionButtons.deleteButton;

  if (descriptor.parameter == OscillatorParameter::level)
    AttachLevelTabChildren(page, context, styles, descriptor, restoreButton, keyNoteActionButtons.addButton, keyNoteActionButtons.deleteButton, sliderControl);
  else if (descriptor.parameter == OscillatorParameter::breath_power)
    AttachBreathTabChildren(page, context, styles, descriptor, restoreButton, keyNoteActionButtons.addButton, keyNoteActionButtons.deleteButton, sliderControl);
  else if (descriptor.parameter == OscillatorParameter::attack || descriptor.parameter == OscillatorParameter::release)
    AttachAttackReleaseTabChildren(page, context, styles, descriptor, restoreButton, keyNoteActionButtons.addButton, keyNoteActionButtons.deleteButton,
                                   sliderControl);
  else if (descriptor.parameter == OscillatorParameter::pitch)
    AttachPitchTabChildren(page, context, styles, descriptor, restoreButton, keyNoteActionButtons.addButton, keyNoteActionButtons.deleteButton, sliderControl);
  else if (descriptor.parameter == OscillatorParameter::pan)
    AttachPanTabChildren(page, context, styles, descriptor, restoreButton, keyNoteActionButtons.addButton, keyNoteActionButtons.deleteButton, sliderControl);
  else
    AttachVariationTabChildren(page, context, styles, descriptor, restoreButton, keyNoteActionButtons.addButton, keyNoteActionButtons.deleteButton,
                               sliderControl);

  context->RefreshOscillatorTabs();
}

inline IVTabPage* CreateOscillatorTabPage(const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                          const OscillatorTabDescriptor& descriptor) {
  auto* page = new EditorOscillatorTabPage(
      [context, styles, descriptor](IVTabPage* page, const IRECT&) { AttachOscillatorTabChildren(page, context, styles, descriptor); },
      [descriptor](IContainerBase* page, const IRECT& bounds) { ResizeHarmonicOscillatorTabPage(page, bounds, SupportsMacrosMode(descriptor.parameter)); },
      [context, descriptor](bool isVisible) {
        auto* control = context->oscillatorTabControls.sliderControls[static_cast<std::size_t>(descriptor.parameter)];
        if (!control) return;

        context->oscillatorTabControls.macroStates[static_cast<std::size_t>(descriptor.parameter)].midiNote = -1;
        if (isVisible && context->HasValidSelectedMidiNote()) context->CaptureOscillatorRestoreState(descriptor.parameter);
        else
          control->ClearRestoreState();

        context->RefreshOscillatorTabs();
      },
      [context]() { context->ApplyMacrosModeVisibility(); });

  context->oscillatorTabControls.tabPages[static_cast<std::size_t>(descriptor.parameter)] = page;
  return page;
}

inline PageMap CreateOscillatorTabPages(const std::shared_ptr<EditorContext>& context, const EditorStyles& styles) {
  PageMap pages;
  for (const auto& descriptor : GetOscillatorTabDescriptors()) {
    pages.insert({descriptor.title, CreateOscillatorTabPage(context, styles, descriptor)});
    if (descriptor.parameter == OscillatorParameter::level) pages.insert({kEqTabTitle, CreateEqTabPage(context, styles)});
  }
  return pages;
}

inline void RestoreSelectedTab(IContainerBase* editorTabsControl, const std::shared_ptr<int>& selectedTabIndex) {
  if (!editorTabsControl) return;

  auto* tabSwitch = editorTabsControl->NChildren() > 0 ? editorTabsControl->GetChild(0)->As<IVTabSwitchControl>() : nullptr;
  if (!tabSwitch) return;

  const auto originalAction = tabSwitch->GetActionFunction();
  tabSwitch->SetActionFunction([originalAction, selectedTabIndex](IControl* caller) {
    if (originalAction) originalAction(caller);

    if (auto* switchControl = caller ? caller->As<IVTabSwitchControl>() : nullptr) *selectedTabIndex = switchControl->GetSelectedIdx();

    if (caller && caller->GetUI() && caller->GetUI()->TooltipsEnabled()) caller->GetUI()->UpdateTooltips();
  });

  const int maxTabIndex = GetEditorTabCount() - 1;
  *selectedTabIndex = std::clamp(*selectedTabIndex, 0, maxTabIndex);
  tabSwitch->SetValue(maxTabIndex > 0 ? static_cast<double>(*selectedTabIndex) / static_cast<double>(maxTabIndex) : 0.0);

  if (const auto tabSwitchAction = tabSwitch->GetActionFunction()) tabSwitchAction(tabSwitch);
  tabSwitch->SetDirty(false);
}

inline void BindEditorState(EditorContext& context, const std::shared_ptr<EditorState>& editorState) {
  context.model.patchMutex = {editorState, &editorState->patchMutex};
  context.model.compoundPatch = {editorState, &editorState->compoundPatch};
  context.model.breathCCSource = {editorState, &editorState->breathCCSource};
  context.model.portamentoCC = {editorState, &editorState->portamentoCC};
  context.model.pitchBendRange = {editorState, &editorState->pitchBendRange};
  context.model.harmonicVisualizerEnabled = {editorState, &editorState->harmonicVisualizerEnabled};
  context.model.selectedMidiNote = {editorState, &editorState->selectedMidiNote};
  context.model.selectedTabIndex = {editorState, &editorState->selectedTabIndex};
  context.model.editMode = {editorState, &editorState->editMode};
  context.model.oscillatorEditModes = {editorState, &editorState->oscillatorEditModes};
  context.model.oscillatorEditScopes = {editorState, &editorState->oscillatorEditScopes};
  context.oscillatorView.xRangeMin = {editorState, &editorState->oscillatorXRangeMin};
  context.oscillatorView.xRangeMax = {editorState, &editorState->oscillatorXRangeMax};
  context.oscillatorView.transforms = {editorState, &editorState->oscillatorTransforms};
}

inline std::shared_ptr<EditorContext> CreateEditorContext(const std::shared_ptr<EditorState>& editorState, int editorTabsTag) {
  auto context = std::make_shared<EditorContext>();
  context->editorTabsTag = editorTabsTag;
  BindEditorState(*context, editorState);

  *context->oscillatorView.xRangeMin = std::clamp(*context->oscillatorView.xRangeMin, 1, SimplePatch::kNumOscillators);
  *context->oscillatorView.xRangeMax = std::clamp(*context->oscillatorView.xRangeMax, *context->oscillatorView.xRangeMin, SimplePatch::kNumOscillators);
  return context;
}
} // namespace editor

inline std::shared_ptr<editor::EditorContext> AttachEditorMainControls(IGraphics* pGraphics, const std::shared_ptr<EditorState>& editorState,
                                                                       int harmonicVisualizerTag, int editorTabsTag) {
  using namespace editor;

  const EditorStyles styles{};

  auto context = CreateEditorContext(editorState, editorTabsTag);

  pGraphics->AttachControl(new HarmonicVisualizerControl(positions::kHarmonicVisualizer), harmonicVisualizerTag);

  auto* editorTabsControl = new EditorTabbedPagesControl(positions::kEditorTabs, CreateOscillatorTabPages(context, styles), "", styles.tabsStyle, 20.f, 1.f);
  pGraphics->AttachControl(editorTabsControl, editorTabsTag);
  RestoreSelectedTab(editorTabsControl, context->model.selectedTabIndex);
  context->RefreshOscillatorTabs();
  if (pGraphics->TooltipsEnabled()) pGraphics->UpdateTooltips();
  return context;
}

inline void HandleQwertyMidi(IGraphics* pGraphics, int keyboardTag, int& lastQwertyMIDINote, const IMidiMsg& msg) {
  auto* keyboard = pGraphics->GetControlWithTag(keyboardTag)->As<IVKeyboardControl>();
  const int note = msg.NoteNumber();
  const bool noteOn = (msg.StatusMsg() == IMidiMsg::kNoteOn) && (msg.Velocity() > 0);

  if (noteOn) {
    if (lastQwertyMIDINote >= 0 && lastQwertyMIDINote != note) keyboard->SetNoteFromMidi(lastQwertyMIDINote, false);

    keyboard->SetNoteFromMidi(note, true);
    lastQwertyMIDINote = note;
  } else if (note == lastQwertyMIDINote) {
    keyboard->SetNoteFromMidi(note, false);
    lastQwertyMIDINote = -1;
  }
}
} // namespace plugin_ui

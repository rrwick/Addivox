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

  EditorOscillatorTabPage(TabAttachFunc attachFunc, ResizeFunc resizeFunc, VisibilityChangedFunc visibilityChangedFunc)
      : IVTabPage(std::move(attachFunc), std::move(resizeFunc)), mVisibilityChangedFunc(std::move(visibilityChangedFunc)) {}

  void Hide(bool hide) override {
    const bool wasHidden = IsHidden();
    IVTabPage::Hide(hide);
    const bool isHidden = IsHidden();
    if (wasHidden != isHidden && mVisibilityChangedFunc) mVisibilityChangedFunc(!isHidden);
  }

private:
  VisibilityChangedFunc mVisibilityChangedFunc{};
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

  ~EditorTabbedPagesControl() { mPages.Empty(false); }

  void Hide(bool hide) override {
    if (hide) {
      ForAllChildrenFunc([hide](int childIdx, IControl* child) { child->Hide(hide); });
    } else {
      ForAllPagesFunc([](IVTabPage* page) { page->Hide(true); });

      GetTabSwitchControl()->Hide(false);
      GetPage(GetTabSwitchControl()->GetSelectedIdx())->Hide(false);
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

    ForAllPagesFunc([&](IVTabPage* page) {
      AddChildControl(page);
      page->SetTargetAndDrawRECTs(GetPageArea());
      page->Hide(true);
    });

    GetTabSwitchControl()->Hide(false);
    GetPage(0)->Hide(false);
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

      ForAllPagesFunc([&](IVTabPage* page) { page->SetTargetAndDrawRECTs(GetPageArea()); });
    }
  }

  float GetTabHeight() const { return mTabBarHeight; }

  IRECT GetPageArea() const { return mWidgetBounds.GetReducedFromTop(GetTabHeight()); }

  IRECT GetTabBarArea() const { return mWidgetBounds.GetFromTop(GetTabHeight()).FracRectHorizontal(mTabBarFrac, mTabsAlign == EAlign::Far); }

private:
  void AddPage(const char* pageName, const char* pageTooltip, IVTabPage* page) {
    page->SetLabelStr(pageName);
    mPageNames.push_back(pageName);
    mPageTooltips.push_back(pageTooltip ? pageTooltip : "");
    mPages.Add(page);
  }

  void ForAllPagesFunc(const std::function<void(IVTabPage* page)>& func) {
    for (int i = 0; i < mPages.GetSize(); ++i) func(mPages.Get(i));
  }

  EditorTooltipTabSwitchControl* GetTabSwitchControl() { return GetChild(0)->As<EditorTooltipTabSwitchControl>(); }

  IVTabPage* GetPage(int pageIndex) { return mPages.Get(pageIndex); }

  void ShowSelectedPage() {
    ForAllPagesFunc([&](IVTabPage* page) {
      const bool hide = std::strcmp(GetTabSwitchControl()->GetSelectedLabelStr(), page->GetLabelStr()) != 0;
      page->Hide(hide);
    });

    if (IBubbleControl* bubbleControl = GetUI()->GetBubbleControl()) bubbleControl->Hide(true);
  }

  WDL_PtrList<IVTabPage> mPages;
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

inline const OscillatorTabDescriptor* GetSelectedOscillatorTabDescriptor(const std::shared_ptr<EditorContext>& context) {
  if (!context) return nullptr;

  const auto& tabTitles = GetEditorTabTitlesInDisplayOrder();
  const int selectedTabIndex = context->SelectedTabIndex();
  if (selectedTabIndex < 0 || selectedTabIndex >= static_cast<int>(tabTitles.size())) return nullptr;

  const char* selectedTitle = tabTitles[static_cast<std::size_t>(selectedTabIndex)];
  if (!selectedTitle || std::strcmp(selectedTitle, kEqTabTitle) == 0) return nullptr;

  return FindOscillatorTabDescriptorForTitle(selectedTitle);
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

    auto* editorControl = context->eqTab.editorControl ? *context->eqTab.editorControl : nullptr;
    if (!editorControl) return;

    context->ApplyEqCurveActionToSelectedKeyNote(editorControl, [actionName](EqCurve& curve) { return ApplyEqAction(curve, actionName); });
    return;
  }

  const auto* descriptor = GetSelectedOscillatorTabDescriptor(context);
  if (!descriptor) return;

  const char* actionName = GetEditorActionShortcutActionName(descriptor->parameter, keyVK);
  if (!actionName) return;

  const auto parameterIndex = static_cast<std::size_t>(descriptor->parameter);
  auto* sliderControl = (*context->oscillatorTabControls.sliderControls)[parameterIndex];
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

inline void AttachDefaultTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                     const OscillatorTabDescriptor& descriptor, IVButtonControl* restoreButton, IVButtonControl* addButton,
                                     IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);
  auto* editModeControl = CreateEditModeControl(context->model.oscillatorEditModes, descriptor, styles);
  auto* editModeScopeControl = CreateEditModeScopeControl(context->model.oscillatorEditScopes, descriptor, styles);
  page->AddChildControl(CreateUtilityLabelControl("X range:", styles));
  page->AddChildControl(xRangeControls.minControl);
  page->AddChildControl(xRangeControls.maxControl);
  page->AddChildControl(CreateEditModeLabelControl(styles));
  page->AddChildControl(editModeControl);
  page->AddChildControl(editModeScopeControl);
  page->AddChildControl(allKeyNotesControls.toggleControl);
  page->AddChildControl(allKeyNotesControls.labelControl);
  page->AddChildControl(restoreButton);
  page->AddChildControl(addButton);
  page->AddChildControl(deleteButton);
  page->AddChildControl(sliderControl);
}

inline void AttachOscillatorTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                        const OscillatorTabDescriptor& descriptor) {
  auto* restoreButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Restore", styles.restoreButtonStyle, true, false);
  restoreButton->SetTooltip(help_text::oscillator_tabs::kRestoreButton);
  restoreButton->SetAnimationEndActionFunction([context, descriptor](IControl* caller) { RestoreOscillatorTabValues(context, caller, descriptor); });
  const auto keyNoteActionButtons = CreateKeyNoteActionButtons(context, styles);

  auto* sliderControl = CreateOscillatorSliderControl(context, descriptor, styles);
  const auto parameterIndex = static_cast<std::size_t>(descriptor.parameter);
  (*context->oscillatorTabControls.sliderControls)[parameterIndex] = sliderControl;
  (*context->oscillatorTabControls.restoreButtons)[parameterIndex] = restoreButton;
  (*context->oscillatorTabControls.addButtons)[parameterIndex] = keyNoteActionButtons.addButton;
  (*context->oscillatorTabControls.deleteButtons)[parameterIndex] = keyNoteActionButtons.deleteButton;

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
  else if (IsVariationParameter(descriptor.parameter))
    AttachVariationTabChildren(page, context, styles, descriptor, restoreButton, keyNoteActionButtons.addButton, keyNoteActionButtons.deleteButton,
                               sliderControl);
  else
    AttachDefaultTabChildren(page, context, styles, descriptor, restoreButton, keyNoteActionButtons.addButton, keyNoteActionButtons.deleteButton,
                             sliderControl);

  context->RefreshOscillatorTabs();
}

inline IVTabPage* CreateOscillatorTabPage(const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                          const OscillatorTabDescriptor& descriptor) {
  auto resizeFunc = UsesHarmonicOscillatorTabLayout(descriptor.parameter) ? ResizeHarmonicOscillatorTabPage : ResizeDefaultOscillatorTabPage;

  return new EditorOscillatorTabPage(
      [context, styles, descriptor](IVTabPage* page, const IRECT&) { AttachOscillatorTabChildren(page, context, styles, descriptor); }, resizeFunc,
      [context, descriptor](bool isVisible) {
        auto* control = (*context->oscillatorTabControls.sliderControls)[static_cast<std::size_t>(descriptor.parameter)];
        if (!control) return;

        if (isVisible) {
          if (context->HasValidSelectedMidiNote()) control->CaptureRestoreState(context->SelectedMidiNote());
          else
            control->ClearRestoreState();
        } else
          control->ClearRestoreState();

        context->RefreshOscillatorTabs();
      });
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

inline std::shared_ptr<EditorContext> CreateEditorContext(const std::shared_ptr<EditorState>& editorState, int editorTabsTag) {
  auto context = std::make_shared<EditorContext>();
  context->editorTabsTag = editorTabsTag;
  context->model.patchMutex = std::shared_ptr<std::recursive_mutex>(editorState, &editorState->patchMutex);
  context->model.compoundPatch = std::shared_ptr<CompoundPatch>(editorState, &editorState->compoundPatch);
  context->model.breathCCSource = std::shared_ptr<BreathCCSource>(editorState, &editorState->breathCCSource);
  context->model.pitchBendRange = std::shared_ptr<int>(editorState, &editorState->pitchBendRange);
  context->model.harmonicVisualizerEnabled = std::shared_ptr<bool>(editorState, &editorState->harmonicVisualizerEnabled);
  context->model.selectedMidiNote = std::shared_ptr<int>(editorState, &editorState->selectedMidiNote);
  context->model.selectedTabIndex = std::shared_ptr<int>(editorState, &editorState->selectedTabIndex);
  context->model.editMode = std::shared_ptr<bool>(editorState, &editorState->editMode);
  context->model.oscillatorEditModes =
      std::shared_ptr<std::array<EditorOscillatorEditMode, OscillatorSettings::kNumParameters>>(editorState, &editorState->oscillatorEditModes);
  context->model.oscillatorEditScopes =
      std::shared_ptr<std::array<EditorOscillatorEditScope, OscillatorSettings::kNumParameters>>(editorState, &editorState->oscillatorEditScopes);
  context->oscillatorView.xRangeMin = std::shared_ptr<int>(editorState, &editorState->oscillatorXRangeMin);
  context->oscillatorView.xRangeMax = std::shared_ptr<int>(editorState, &editorState->oscillatorXRangeMax);
  context->levelTab.levelTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->levelTransform);
  context->breathTab.breathTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->breathTransform);
  context->pitchTab.pitchTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->pitchTransform);
  context->panTab.panTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->panTransform);
  context->variationTab.transforms = std::shared_ptr<std::array<EditorLevelTransform, 6>>(editorState, &editorState->variationTransforms);
  context->attackReleaseTab.attackTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->attackTransform);
  context->attackReleaseTab.releaseTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->releaseTransform);
  context->oscillatorTabControls.sliderControls = std::make_shared<std::array<OscillatorSliderControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.sliderControls->fill(nullptr);
  context->oscillatorTabControls.xRangeMinControls = std::make_shared<std::array<IVNumberBoxControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.xRangeMinControls->fill(nullptr);
  context->oscillatorTabControls.xRangeMaxControls = std::make_shared<std::array<IVNumberBoxControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.xRangeMaxControls->fill(nullptr);
  context->oscillatorTabControls.allKeyNotesToggles = std::make_shared<std::array<IVToggleControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.allKeyNotesToggles->fill(nullptr);
  context->oscillatorTabControls.restoreButtons = std::make_shared<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.restoreButtons->fill(nullptr);
  context->oscillatorTabControls.addButtons = std::make_shared<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.addButtons->fill(nullptr);
  context->oscillatorTabControls.deleteButtons = std::make_shared<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.deleteButtons->fill(nullptr);
  context->levelTab.setShapeControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->levelTab.actionsControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->breathTab.setShapeControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->breathTab.actionsControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->pitchTab.setShapeControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->pitchTab.actionsControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->panTab.setShapeControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->panTab.actionsControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->variationTab.setShapeControls = std::make_shared<std::array<ActionSelectionControl*, 6>>();
  context->variationTab.setShapeControls->fill(nullptr);
  context->variationTab.actionsControls = std::make_shared<std::array<ActionSelectionControl*, 6>>();
  context->variationTab.actionsControls->fill(nullptr);
  context->attackReleaseTab.setShapeControls = std::make_shared<std::array<ActionSelectionControl*, 2>>();
  context->attackReleaseTab.setShapeControls->fill(nullptr);
  context->attackReleaseTab.actionsControls = std::make_shared<std::array<ActionSelectionControl*, 2>>();
  context->attackReleaseTab.actionsControls->fill(nullptr);
  context->eqTab.setShapeControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->eqTab.actionsControl = std::make_shared<ActionSelectionControl*>(nullptr);
  context->eqTab.allKeyNotesToggle = std::make_shared<IVToggleControl*>(nullptr);
  context->eqTab.restoreButton = std::make_shared<IVButtonControl*>(nullptr);
  context->eqTab.addButton = std::make_shared<IVButtonControl*>(nullptr);
  context->eqTab.deleteButton = std::make_shared<IVButtonControl*>(nullptr);
  context->eqTab.editorControl = std::make_shared<EqEditorControl*>(nullptr);
  context->keyboardControl = std::make_shared<KeyboardControl*>(nullptr);
  context->title.patchManagerControl = std::make_shared<IControl*>(nullptr);

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

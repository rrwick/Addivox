#pragma once

#include "IControls.h"
#include "keyboard_control.h"
#include "editor_tabs/common.h"
#include "editor_tabs/level.h"
#include "editor_tabs/breath.h"
#include "editor_tabs/attack_release.h"
#include "editor_tabs/pitch.h"
#include "editor_tabs/pan.h"
#include "editor_tabs/variation.h"
#include "../visualizer/harmonic_visualizer_control.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

class EditorOscillatorTabPage final : public IVTabPage
{
public:
  using VisibilityChangedFunc = std::function<void(bool isVisible)>;

  EditorOscillatorTabPage(TabAttachFunc attachFunc, ResizeFunc resizeFunc, VisibilityChangedFunc visibilityChangedFunc)
  : IVTabPage(std::move(attachFunc), std::move(resizeFunc))
  , mVisibilityChangedFunc(std::move(visibilityChangedFunc))
  {
  }

  void Hide(bool hide) override
  {
    const bool wasHidden = IsHidden();
    IVTabPage::Hide(hide);
    const bool isHidden = IsHidden();
    if(wasHidden != isHidden && mVisibilityChangedFunc)
      mVisibilityChangedFunc(!isHidden);
  }

private:
  VisibilityChangedFunc mVisibilityChangedFunc{};
};

namespace editor
{
inline const std::vector<OscillatorTabDescriptor>& GetOscillatorTabDescriptors()
{
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

inline void AttachDefaultTabChildren(IVTabPage* page,
                                     const std::shared_ptr<EditorContext>& context,
                                     const EditorStyles& styles,
                                     const OscillatorTabDescriptor& descriptor,
                                     IVButtonControl* restoreButton,
                                     OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  page->AddChildControl(MakePassiveControl(new IMultiLineTextControl(IRECT(), descriptor.description, styles.descriptionText, COLOR_TRANSPARENT)));
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "X range:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(xRangeControls.minControl);
  page->AddChildControl(xRangeControls.maxControl);
  page->AddChildControl(restoreButton);
  page->AddChildControl(sliderControl);
}

inline void AttachOscillatorTabChildren(IVTabPage* page,
                                        const std::shared_ptr<EditorContext>& context,
                                        const EditorStyles& styles,
                                        const OscillatorTabDescriptor& descriptor)
{
  auto* restoreButton = new IVButtonControl(IRECT(), SplashClickActionFunc, "Restore", styles.restoreButtonStyle, true, false);
  restoreButton->SetAnimationEndActionFunction([context, descriptor](IControl* caller) {
    RestoreOscillatorTabValues(context, caller, descriptor);
  });

  auto* sliderControl = CreateOscillatorSliderControl(context, descriptor, styles);
  const auto parameterIndex = static_cast<std::size_t>(descriptor.parameter);
  (*context->oscillatorTabControls.sliderControls)[parameterIndex] = sliderControl;
  (*context->oscillatorTabControls.restoreButtons)[parameterIndex] = restoreButton;

  if(descriptor.parameter == OscillatorParameter::intensity)
    AttachLevelTabChildren(page, context, styles, descriptor, restoreButton, sliderControl);
  else if(descriptor.parameter == OscillatorParameter::breath_power)
    AttachBreathTabChildren(page, context, styles, descriptor, restoreButton, sliderControl);
  else if(descriptor.parameter == OscillatorParameter::attack
       || descriptor.parameter == OscillatorParameter::release)
    AttachAttackReleaseTabChildren(page, context, styles, descriptor, restoreButton, sliderControl);
  else if(descriptor.parameter == OscillatorParameter::pitch)
    AttachPitchTabChildren(page, context, styles, descriptor, restoreButton, sliderControl);
  else if(descriptor.parameter == OscillatorParameter::pan)
    AttachPanTabChildren(page, context, styles, descriptor, restoreButton, sliderControl);
  else if(IsVariationParameter(descriptor.parameter))
    AttachVariationTabChildren(page, context, styles, descriptor, restoreButton, sliderControl);
  else
    AttachDefaultTabChildren(page, context, styles, descriptor, restoreButton, sliderControl);

  context->RefreshOscillatorTabs();
}

inline IVTabPage* CreateOscillatorTabPage(const std::shared_ptr<EditorContext>& context,
                                          const EditorStyles& styles,
                                          const OscillatorTabDescriptor& descriptor)
{
  auto resizeFunc = ResizeDefaultOscillatorTabPage;
  if(descriptor.parameter == OscillatorParameter::intensity)
    resizeFunc = ResizeLevelTabPage;
  else if(descriptor.parameter == OscillatorParameter::breath_power)
    resizeFunc = ResizeBreathTabPage;
  else if(descriptor.parameter == OscillatorParameter::attack
       || descriptor.parameter == OscillatorParameter::release)
    resizeFunc = ResizeAttackReleaseTabPage;
  else if(descriptor.parameter == OscillatorParameter::pitch)
    resizeFunc = ResizePitchTabPage;
  else if(descriptor.parameter == OscillatorParameter::pan)
    resizeFunc = ResizePanTabPage;
  else if(IsVariationParameter(descriptor.parameter))
    resizeFunc = ResizeVariationTabPage;

  return new EditorOscillatorTabPage(
    [context, styles, descriptor](IVTabPage* page, const IRECT&) {
      AttachOscillatorTabChildren(page, context, styles, descriptor);
    },
    resizeFunc,
    [context, descriptor](bool isVisible) {
      auto* control = (*context->oscillatorTabControls.sliderControls)[static_cast<std::size_t>(descriptor.parameter)];
      if(!control)
        return;

      if(isVisible)
      {
        if(context->HasValidSelectedMidiNote())
          control->CaptureRestoreState(context->SelectedMidiNote());
        else
          control->ClearRestoreState();
      }
      else
        control->ClearRestoreState();

      context->RefreshOscillatorTabs();
    });
}

inline PageMap CreateOscillatorTabPages(const std::shared_ptr<EditorContext>& context,
                                        const EditorStyles& styles)
{
  PageMap pages;
  for(const auto& descriptor : GetOscillatorTabDescriptors())
    pages.insert({descriptor.title, CreateOscillatorTabPage(context, styles, descriptor)});
  return pages;
}

inline void RestoreSelectedTab(IVTabbedPagesControl* editorTabsControl, const std::shared_ptr<int>& selectedTabIndex)
{
  auto* tabSwitch = editorTabsControl->NChildren() > 0 ? editorTabsControl->GetChild(0)->As<IVTabSwitchControl>() : nullptr;
  if(!tabSwitch)
    return;

  const auto originalAction = tabSwitch->GetActionFunction();
  tabSwitch->SetActionFunction([originalAction, selectedTabIndex](IControl* caller) {
    if(originalAction)
      originalAction(caller);

    if(auto* switchControl = caller ? caller->As<IVTabSwitchControl>() : nullptr)
      *selectedTabIndex = switchControl->GetSelectedIdx();
  });

  const int maxTabIndex = static_cast<int>(GetOscillatorTabDescriptors().size()) - 1;
  *selectedTabIndex = std::clamp(*selectedTabIndex, 0, maxTabIndex);
  tabSwitch->SetValue(maxTabIndex > 0 ? static_cast<double>(*selectedTabIndex) / static_cast<double>(maxTabIndex) : 0.0);

  if(const auto tabSwitchAction = tabSwitch->GetActionFunction())
    tabSwitchAction(tabSwitch);
  tabSwitch->SetDirty(false);
}

inline std::shared_ptr<EditorContext> CreateEditorContext(const std::shared_ptr<EditorState>& editorState,
                                                          int editorTabsTag)
{
  auto context = std::make_shared<EditorContext>();
  context->editorTabsTag = editorTabsTag;
  context->model.compoundPreset = std::shared_ptr<CompoundPreset>(editorState, &editorState->compoundPreset);
  context->model.selectedMidiNote = std::shared_ptr<int>(editorState, &editorState->selectedMidiNote);
  context->model.selectedTabIndex = std::shared_ptr<int>(editorState, &editorState->selectedTabIndex);
  context->model.editMode = std::shared_ptr<bool>(editorState, &editorState->editMode);
  context->oscillatorView.xRangeMin = std::shared_ptr<int>(editorState, &editorState->oscillatorXRangeMin);
  context->oscillatorView.xRangeMax = std::shared_ptr<int>(editorState, &editorState->oscillatorXRangeMax);
  context->levelTab.levelTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->levelTransform);
  context->breathTab.breathTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->breathTransform);
  context->pitchTab.pitchTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->pitchTransform);
  context->panTab.panTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->panTransform);
  context->variationTab.transforms = std::shared_ptr<std::array<EditorLevelTransform, 6>>(editorState, &editorState->variationTransforms);
  context->attackReleaseTab.attackTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->attackTransform);
  context->attackReleaseTab.releaseTransform = std::shared_ptr<EditorLevelTransform>(editorState, &editorState->releaseTransform);
  context->oscillatorTabControls.sliderControls =
    std::make_shared<std::array<OscillatorSliderControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.sliderControls->fill(nullptr);
  context->oscillatorTabControls.xRangeMinControls =
    std::make_shared<std::array<IVNumberBoxControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.xRangeMinControls->fill(nullptr);
  context->oscillatorTabControls.xRangeMaxControls =
    std::make_shared<std::array<IVNumberBoxControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.xRangeMaxControls->fill(nullptr);
  context->oscillatorTabControls.restoreButtons =
    std::make_shared<std::array<IVButtonControl*, OscillatorSettings::kNumParameters>>();
  context->oscillatorTabControls.restoreButtons->fill(nullptr);
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
  context->buttons.addButton = std::make_shared<IVButtonControl*>(nullptr);
  context->buttons.deleteButton = std::make_shared<IVButtonControl*>(nullptr);

  *context->oscillatorView.xRangeMin = std::clamp(*context->oscillatorView.xRangeMin, 1, SimplePreset::kNumOscillators);
  *context->oscillatorView.xRangeMax =
    std::clamp(*context->oscillatorView.xRangeMax, *context->oscillatorView.xRangeMin, SimplePreset::kNumOscillators);
  return context;
}
} // namespace editor

inline std::shared_ptr<editor::EditorContext> AttachEditorMainControls(IGraphics* pGraphics,
                                                                                          const std::shared_ptr<EditorState>& editorState,
                                                                                          int harmonicVisualizerTag,
                                                                                          int editorTabsTag,
                                                                                          int breathMeterTag)
{
  using namespace editor;

  const IRECT breathMeterBounds = IRECT::MakeXYWH(12.f, 113.f, 20.f, 266.f);
  const IRECT harmonicVisualizerBounds = IRECT::MakeXYWH(38.f, 74.f, 798.f, 344.f);
  const IRECT editorTabsBounds = IRECT::MakeXYWH(12.f, 74.f, 824.f, 344.f);
  const IVStyle meterStyle = theme::MeterStyle();
  const EditorStyles styles{};

  auto context = CreateEditorContext(editorState, editorTabsTag);

  pGraphics->AttachControl(new IVMeterControl<1>(breathMeterBounds, "", meterStyle), breathMeterTag);
  pGraphics->AttachControl(new HarmonicVisualizerControl(harmonicVisualizerBounds), harmonicVisualizerTag);

  auto* editorTabsControl = new IVTabbedPagesControl(
    editorTabsBounds,
    CreateOscillatorTabPages(context, styles),
    "",
    styles.tabsStyle,
    20.f,
    1.f);
  pGraphics->AttachControl(editorTabsControl, editorTabsTag);
  RestoreSelectedTab(editorTabsControl, context->model.selectedTabIndex);
  context->RefreshOscillatorTabs();
  return context;
}

inline void HandleQwertyMidi(IGraphics* pGraphics, int keyboardTag, int& lastQwertyMIDINote, const IMidiMsg& msg)
{
  auto* keyboard = pGraphics->GetControlWithTag(keyboardTag)->As<IVKeyboardControl>();
  const int note = msg.NoteNumber();
  const bool noteOn = (msg.StatusMsg() == IMidiMsg::kNoteOn) && (msg.Velocity() > 0);

  if(noteOn)
  {
    if(lastQwertyMIDINote >= 0 && lastQwertyMIDINote != note)
      keyboard->SetNoteFromMidi(lastQwertyMIDINote, false);

    keyboard->SetNoteFromMidi(note, true);
    lastQwertyMIDINote = note;
  }
  else if(note == lastQwertyMIDINote)
  {
    keyboard->SetNoteFromMidi(note, false);
    lastQwertyMIDINote = -1;
  }
}
} // namespace plugin_ui

cmake_minimum_required(VERSION 3.14)
project(Addivox VERSION 1.0.0)

option(ADDIVOX_DEMO "Build the Addivox demo version" OFF)

if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

file(GLOB BUILTIN_PATCH_RESOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/factory_patches/*.toml")

set(ADDIVOX_COMMON_SOURCES
  src/demo_mode.h
  src/synth/voice.cpp
  src/synth/voice.h
  src/synth/oscillator.cpp
  src/synth/oscillator.h
  src/dsp/shared.h
  src/dsp/gradient_noise.h
  src/visualizer/harmonic_visualizer_frame.h
  src/visualizer/harmonic_visualizer_sender.h
  src/visualizer/harmonic_visualizer_control.h
  src/settings/oscillator.cpp
  src/settings/oscillator.h
  src/settings/global.cpp
  src/settings/global.h
  src/settings/effects.cpp
  src/settings/effects.h
  src/effects/drive.cpp
  src/effects/drive.h
  src/effects/tone.cpp
  src/effects/tone.h
  src/effects/chorus.cpp
  src/effects/chorus.h
  src/effects/reverb.cpp
  src/effects/reverb.h
  src/synth/midi_synth.h
  src/synth/synth_engine.h
  src/settings/patch_io.h
  src/settings/eq.h
  src/settings/params.h
  src/midi/breath_control.h
)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    src/plugin.cpp
    src/plugin.h
    ${ADDIVOX_COMMON_SOURCES}
    src/ui/transformations.cpp
    src/ui/transformations.h
    src/ui/oscillator_slider_control.h
    src/ui/number_box_control.h
    src/ui/patch_manager.h
    src/ui/layout.h
    src/ui/theme.h
    src/ui/about_built_with_control.h
    src/ui/action_selection_control.h
    src/ui/colour.h
    src/ui/control_utils.h
    src/ui/editor_messages.h
    src/ui/editor_panel.h
    src/ui/editor_state.h
    src/ui/editor_tabs/attack_release.h
    src/ui/editor_tabs/breath.h
    src/ui/editor_tabs/common.h
    src/ui/editor_tabs/eq.h
    src/ui/editor_tabs/level.h
    src/ui/editor_tabs/pan.h
    src/ui/editor_tabs/pitch.h
    src/ui/editor_tabs/variation.h
    src/ui/eq_editor.h
    src/ui/help_text.h
    src/ui/keyboard_control.h
    src/ui/knob.h
    src/ui/peak_led_meter_control.h
    src/ui/pitch_bend_wheel.h
    src/ui/positions.h
    src/ui/settings_menu_button.h
    src/ui/touch_range_slider_control.h
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
    resources/fonts/Roboto-Bold.ttf
    resources/fonts/Roboto-Black.ttf
    ${BUILTIN_PATCH_RESOURCES}
    resources/img/knob.svg
    resources/img/knob-fixed.svg
    resources/img/knob-rotating.svg
    resources/img/background.svg
    resources/img/about-logo.svg
    resources/img/gear.svg
)

if(APPLE AND NOT IOS)
  add_executable(addivox-cli
    src/cli/cli_main.cpp
    src/cli/headless_renderer.cpp
    src/cli/headless_renderer.h
    src/cli/midi_file_parser.cpp
    src/cli/midi_file_parser.h
    ${ADDIVOX_COMMON_SOURCES}
  )

  target_include_directories(addivox-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IPLUG2_DIR}/IPlug
    ${IPLUG2_DIR}/IPlug/Extras
    ${IPLUG2_DIR}/WDL
  )

  set_target_properties(addivox-cli PROPERTIES
    OUTPUT_NAME $<IF:$<BOOL:${ADDIVOX_DEMO}>,addivox-demo,addivox>
    CXX_STANDARD ${IPLUG2_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )

  target_compile_definitions(addivox-cli PRIVATE ADDIVOX_DEMO=$<IF:$<BOOL:${ADDIVOX_DEMO}>,1,0>)
endif()

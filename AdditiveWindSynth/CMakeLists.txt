cmake_minimum_required(VERSION 3.14)
project(AdditiveWindSynth VERSION 1.0.0)

if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

file(GLOB BUILTIN_PRESET_RESOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/presets/*.toml")

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    src/plugin.cpp
    src/plugin.h
    src/synth/voice.cpp
    src/synth/voice.h
    src/synth/oscillator.cpp
    src/synth/oscillator.h
    src/visualizer/harmonic_visualizer_frame.h
    src/visualizer/harmonic_visualizer_sender.h
    src/visualizer/harmonic_visualizer_control.h
    src/settings/settings_oscillator.cpp
    src/settings/settings_oscillator.h
    src/settings/settings_global.cpp
    src/settings/settings_global.h
    src/settings/settings_effects.cpp
    src/settings/settings_effects.h
    src/effects/warmth.cpp
    src/effects/warmth.h
    src/effects/reverb.cpp
    src/effects/reverb.h
    src/ui/transformations.cpp
    src/ui/transformations.h
    src/ui/oscillator_slider_control.cpp
    src/ui/oscillator_slider_control.h
    src/synth/midi_synth.h
    src/synth/synth_engine.h
    src/settings/preset_io.h
    src/ui/layered_svg_knob_control.h
    src/ui/layout.h
    src/ui/theme.h
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
    resources/fonts/Roboto-Bold.ttf
    resources/fonts/Roboto-Black.ttf
    ${BUILTIN_PRESET_RESOURCES}
    assets/knob.svg
    assets/knob-fixed.svg
    assets/knob-rotating.svg
    assets/background.svg
)

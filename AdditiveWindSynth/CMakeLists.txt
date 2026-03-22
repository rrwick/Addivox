cmake_minimum_required(VERSION 3.14)
project(AdditiveWindSynth VERSION 1.0.0)

if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

file(GLOB BUILTIN_PRESET_RESOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.toml")

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    src/plugin.cpp
    src/plugin.h
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
    src/ui/transformations.cpp
    src/ui/transformations.h
    src/ui/oscillator_slider_control.h
    src/ui/number_box_control.h
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

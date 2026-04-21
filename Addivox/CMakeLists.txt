cmake_minimum_required(VERSION 3.14)
project(Addivox VERSION 1.0.0)

if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

file(GLOB BUILTIN_PRESET_RESOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.toml")

set(ADDIVOX_COMMON_SOURCES
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
  src/settings/preset_io.h
)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    src/plugin.cpp
    src/plugin.h
    ${ADDIVOX_COMMON_SOURCES}
    src/ui/transformations.cpp
    src/ui/transformations.h
    src/ui/oscillator_slider_control.h
    src/ui/noise_band_slider_control.h
    src/ui/number_box_control.h
    src/ui/layout.h
    src/ui/theme.h
    src/ui/editor_tabs/noise_sustain.h
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
    assets/about-logo.svg
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
    OUTPUT_NAME addivox
    CXX_STANDARD ${IPLUG2_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )
endif()

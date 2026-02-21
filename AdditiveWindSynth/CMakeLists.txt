cmake_minimum_required(VERSION 3.14)
project(AdditiveWindSynth VERSION 1.0.0)

if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    src/plugin.cpp
    src/plugin.h
    src/voice.cpp
    src/voice.h
    src/oscillator.cpp
    src/oscillator.h
    src/settings.cpp
    src/settings.h
    src/midi_synth.h
    src/synth_engine.h
    src/presets.h
    src/ui_layout.h
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
)

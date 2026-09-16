cmake_minimum_required(VERSION 3.22)
project(FORGE64 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FORGE64_BUILD_CLAP "Also build the CLAP target (clap-juce-extensions)" ON)
option(FORGE64_SCRIPTING  "Embed the Lua script engine" ON)

include(FetchContent)

# Avoid extra system deps (webkit/curl/jack) - not needed by FORGE64.
set(JUCE_WEB_BROWSER OFF)
set(JUCE_USE_CURL OFF)
set(JUCE_JACK OFF)

# ---------------------------------------------------------------- JUCE
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.8
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- Lua
if(FORGE64_SCRIPTING)
  FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cmake-not-used)
  FetchContent_MakeAvailable(lua)

  file(GLOB F64_LUA_SOURCES ${lua_SOURCE_DIR}/*.c)
  list(FILTER F64_LUA_SOURCES EXCLUDE REGEX ".*/(lua|luac|onelua)\\.c$")
  add_library(lua54 STATIC ${F64_LUA_SOURCES})
  target_include_directories(lua54 PUBLIC ${lua_SOURCE_DIR})
  if(UNIX AND NOT APPLE)
    target_compile_definitions(lua54 PUBLIC LUA_USE_LINUX)
    target_link_libraries(lua54 PUBLIC ${CMAKE_DL_LIBS} m)
  endif()
endif()

# ---------------------------------------------------------------- Plugin
juce_add_plugin(Forge64
  COMPANY_NAME               "Forge Audio"
  IS_SYNTH                   TRUE
  NEEDS_MIDI_INPUT           TRUE
  PRODUCES_MIDI_OUTPUT       FALSE
  IS_MIDI_EFFECT             FALSE
  EDITOR_WANT_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD    FALSE
  PLUGIN_MANUFACTURER_CODE   Frg6
  PLUGIN_CODE                Fg64
  FORMATS                    VST3 Standalone
  VST3_CATEGORIES            Instrument Drum Sampler Synthesizer
  PRODUCT_NAME               "FORGE64")

juce_generate_juce_header(Forge64)

target_sources(Forge64 PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Engine/PadGrid.cpp
  Source/Engine/VoicePool.cpp
  Source/Engine/PadChain.cpp
  Source/Modulation/ModSources.cpp
  Source/Modulation/ModMatrix.cpp
  Source/Scripting/LuaEngine.cpp
  Source/Presets/PresetManager.cpp
  Source/UI/ModRingKnob.cpp
  Source/UI/PadGridView.cpp
  Source/UI/PadEditor.cpp
  Source/UI/ModPanel.cpp)

target_compile_definitions(Forge64 PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0)

target_link_libraries(Forge64 PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  juce::juce_codeeditor
  juce::juce_recommended_config_flags)

if(FORGE64_SCRIPTING)
  target_compile_definitions(Forge64 PRIVATE FORGE64_SCRIPTING=1)
  target_link_libraries(Forge64 PRIVATE lua54)
endif()

# ---------------------------------------------------------------- CLAP
if(FORGE64_BUILD_CLAP)
  FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main)
  FetchContent_MakeAvailable(clap-juce-extensions)

  clap_juce_extensions(JUCE_TARGET Forge64
    CLAP_ID          "com.forgeaudio.forge64"
    CLAP_FEATURES    instrument "drum machine" synthesizer
    CLAP_DESCRIPTION "FORGE64 - modular 64-pad drum engine"
    CLAP_MANUAL_URL  "https://forge64.audio/manual"
    CLAP_SUPPORT_URL "https://forge64.audio/support")
endif()

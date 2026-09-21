cmake_minimum_required(VERSION 3.22)
project(CamelClone VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Expects JUCE checked out as a sibling folder, e.g.:
#   git clone --depth 1 https://github.com/juce-framework/JUCE.git
# either right next to this CMakeLists.txt, or point JUCE_PATH elsewhere:
#   cmake -B build -DJUCE_PATH=/path/to/JUCE
if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()
add_subdirectory(${JUCE_PATH} JUCE)

juce_add_plugin(CamelClone
    COMPANY_NAME "YourCompanyName"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ycmp
    PLUGIN_CODE Ccl1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "CamelClone"
)
# NOTE: AU only actually builds on macOS (needs the Audio Unit SDK that
# ships with Xcode) -- JUCE's CMake support skips it automatically on
# Linux/Windows, so listing it here is safe on every platform, it just
# won't produce an .component outside of macOS.

target_sources(CamelClone PRIVATE
    Source/PluginProcessor.cpp
)

target_compile_definitions(CamelClone PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(CamelClone PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

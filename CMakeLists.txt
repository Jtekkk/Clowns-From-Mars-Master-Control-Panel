cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  CLOWNS FROM MARS · MASTER CONTROL PANEL  —  Intelligent analog-modelled mastering processor
#  "Clowns From Mars" master control panel
# ============================================================================
project(MasterControlPanel VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Interprocedural optimisation (LTO) for release audio builds when supported.
option(TT_ENABLE_LTO "Enable link-time optimisation for release builds" OFF)
# Copy the built plugin into the user plugin folder (disable on CI packaging).
option(TT_COPY_PLUGIN "Copy plugin to the user plugin folder after build" ON)

# ----------------------------------------------------------------------------
#  JUCE
# ----------------------------------------------------------------------------
# By default JUCE 8.0.14 is fetched from GitHub. To build offline against a
# local checkout, configure with:
#     -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
# ----------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
#  Plugin target
# ----------------------------------------------------------------------------
juce_add_plugin(MasterControlPanel
    COMPANY_NAME              "Clowns From Mars"
    COMPANY_WEBSITE           "https://clownsfrommars.audio"
    BUNDLE_ID                 "audio.clownsfrommars.mastercontrolpanel"
    PLUGIN_MANUFACTURER_CODE  Cfm0
    PLUGIN_CODE               Cmcp
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "Clowns From Mars Master Control Panel"
    DESCRIPTION               "Intelligent analog-modelled mastering processor"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   ${TT_COPY_PLUGIN}
    VST3_CATEGORIES           "Fx" "Mastering" "EQ" "Dynamics"
    AU_MAIN_TYPE              "kAudioUnitType_Effect"
    VST_NUM_MIDI_INPUTS       0
    VST_NUM_MIDI_OUTPUTS      0)

juce_generate_juce_header(MasterControlPanel)

# ----------------------------------------------------------------------------
#  Sources
# ----------------------------------------------------------------------------
target_sources(MasterControlPanel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/gui/ControlPanelLookAndFeel.cpp
    Source/gui/RotarySlider.cpp
    Source/gui/LevelMeter.cpp
    Source/gui/SpectrumAnalyzer.cpp)

target_include_directories(MasterControlPanel PRIVATE Source)

# ----------------------------------------------------------------------------
#  Compile definitions
# ----------------------------------------------------------------------------
target_compile_definitions(MasterControlPanel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    # DSP quality: keep denormals flushed for predictable low-level behaviour.
    JUCE_DSP_USE_SHARED_FFTW=0)

# Optimise hard, but keep IEEE-correct floating point: -ffast-math would let the
# compiler reorder/contract the DSP and drop NaN/denormal semantics, which is the
# opposite of what a reference-fidelity, double-precision path wants.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(MasterControlPanel PRIVATE
        $<$<CONFIG:Release>:-O3 -fno-math-errno>)
endif()

if(TT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)
    if(_ipo_ok)
        set_target_properties(MasterControlPanel PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# ----------------------------------------------------------------------------
#  JUCE modules
# ----------------------------------------------------------------------------
target_link_libraries(MasterControlPanel PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
# NOTE: juce_recommended_lto_flags is intentionally omitted. On MSVC it enables
# /GL /LTCG which makes an all-of-JUCE build extremely slow with negligible
# real-world DSP benefit. Opt in explicitly with -DTT_ENABLE_LTO=ON if desired.

# ----------------------------------------------------------------------------
#  Internal verification / UI-snapshot tool (developer use only, not shipped)
# ----------------------------------------------------------------------------
option(TT_BUILD_VERIFY "Build the internal verification/snapshot tool" OFF)
if(TT_BUILD_VERIFY)
    add_executable(tt_verify Tools/verify_main.cpp)
    target_link_libraries(tt_verify PRIVATE
        MasterControlPanel
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()

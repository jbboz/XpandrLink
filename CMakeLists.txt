cmake_minimum_required(VERSION 3.22)

# macOS deployment target — must be set BEFORE project() so it applies to all
# targets (replaces patch [1] in post_projucer_patch.sh, which forced 10.13 -> 13.0).
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

project(XpandrLink VERSION 1.0.0 LANGUAGES C CXX)

# Human-facing release label (nav bar + synth-greeting display) -- kept separate from
# the strict numeric PROJECT_VERSION above, which drives AU/VST3 plugin metadata and
# can't carry a pre-release suffix some hosts won't parse. Update this to match the
# current GitHub release tag; PROJECT_VERSION only needs to change for an actual
# version bump, not every pre-release tag.
set(XPANDRLINK_RELEASE_LABEL "1.0.0-beta.2")

# JUCE lives as a sibling directory (same setup as the previous Projucer config).
add_subdirectory(../JUCE "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

# AU is Apple-only; Windows builds Standalone + VST3 only.
if(APPLE)
    set(XPANDRLINK_FORMATS AU VST3 Standalone)
else()
    set(XPANDRLINK_FORMATS VST3 Standalone)
endif()

# Font as binary data (replaces JuceLibraryCode/BinaryData.cpp from Projucer).
# NOTE: juce_add_binary_data strips the hyphen from 'DSEG14Classic-BoldItalic.ttf',
# producing the symbol 'BinaryData::DSEG14ClassicBoldItalic_ttf' (same as Projucer).
juce_add_binary_data(XpandrLinkBinaryData
    SOURCES
        Resources/DSEG14Classic-BoldItalic.ttf
        Resources/InitPatch.syx
)

# Bundled SQLite3 (unchanged — same source file as the Projucer build).
add_library(sqlite3_bundled STATIC
    XpandrLink/ThirdParty/sqlite/sqlite3.c
)
target_include_directories(sqlite3_bundled PUBLIC XpandrLink/ThirdParty/sqlite)

# On Windows, JUCE's default system-wide VST3 folder (C:\Program Files\Common
# Files\VST3) requires admin rights to write to. Redirect to the per-user VST3
# folder instead (still scanned by DAWs) so `cmake --build` works unelevated.
# Must be passed as a juce_add_plugin() argument — JUCE reads VST3_COPY_DIR
# during that call, so setting it via set_target_properties afterward is too late.
if(WIN32)
    set(XPANDRLINK_VST3_COPY_DIR "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
endif()

juce_add_plugin(XpandrLink
    COMPANY_NAME             "Retro Pro Audio"
    BUNDLE_ID                "com.retroproaudio.xpandrlink"
    PLUGIN_MANUFACTURER_CODE "RpAu"
    PLUGIN_CODE              "XpLk"
    FORMATS                  ${XPANDRLINK_FORMATS}
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    VST3_COPY_DIR            "${XPANDRLINK_VST3_COPY_DIR}"
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS "com.apple.security.device.audio-input"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "XpandrLink uses audio input for MIDI device routing."
    ICON_BIG                 "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/icon_512x512.png"
    ICON_SMALL               "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/icon_32x32.png"
    PRODUCT_NAME             "XpandrLink"
    DESCRIPTION              "Oberheim Xpander / Matrix-12 Editor"
    # CRITICAL: explicitly Effect (aufx). JUCE's CMake DEFAULTS to MusicEffect (aumf)
    # whenever NEEDS_MIDI_INPUT is TRUE — but the working project ships aufx, which is
    # what Ableton requires. Do NOT omit this.
    AU_MAIN_TYPE             kAudioUnitType_Effect
    VST3_CATEGORIES          Fx
)

juce_generate_juce_header(XpandrLink)

target_sources(XpandrLink PRIVATE
    XpandrLink/Source/Main.cpp
    XpandrLink/Source/MainComponent.cpp
    XpandrLink/Source/Engine/MidiEngine.cpp
    XpandrLink/Source/Engine/MidiSendQueue.cpp
    XpandrLink/Source/Data/PatchLibrary.cpp
    XpandrLink/Source/Tabs/EditorTabComponent.cpp
    XpandrLink/Source/Tabs/LibraryTabComponent.cpp
    XpandrLink/Source/Plugin/XpandrLinkAudioProcessor.cpp
    XpandrLink/Source/Plugin/XpandrLinkAudioProcessorEditor.cpp
    XpandrLink/Source/UI/HardwareComponents.cpp
    XpandrLink/Source/UI/PatchBrowserPanel.cpp
    XpandrLink/Source/UI/ModSummaryPanel.cpp
    XpandrLink/Source/UI/FullModMatrixPanel.cpp
    XpandrLink/Source/UI/ModulePanel.cpp
    XpandrLink/Source/UI/TitleBarComponent.cpp
)

target_include_directories(XpandrLink PRIVATE XpandrLink/Source)

target_compile_definitions(XpandrLink
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        XPANDRLINK_RELEASE_LABEL="${XPANDRLINK_RELEASE_LABEL}"
)

target_link_libraries(XpandrLink
    PRIVATE
        XpandrLinkBinaryData
        sqlite3_bundled
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Unit tests ─────────────────────────────────────────────────────────────
# Console app that runs BUG01, BUG05, and IDSourceTracking tests.
# Build:  cmake --build build/mac --target XpandrLink_Tests
# Run:    ctest --test-dir build/mac -R XpandrLinkTests -V

juce_add_console_app(XpandrLink_Tests PRODUCT_NAME "XpandrLink Tests")

juce_generate_juce_header(XpandrLink_Tests)

target_sources(XpandrLink_Tests PRIVATE
    XpandrLink/Source/Tests/TestMain.cpp
    XpandrLink/Source/Engine/MidiEngine.cpp
    XpandrLink/Source/Engine/MidiSendQueue.cpp
    XpandrLink/Source/Data/PatchLibrary.cpp
    XpandrLink/Source/UI/ModSummaryPanel.cpp
    XpandrLink/Source/UI/FullModMatrixPanel.cpp
    XpandrLink/Source/UI/HardwareComponents.cpp
)

target_include_directories(XpandrLink_Tests PRIVATE XpandrLink/Source)

target_compile_definitions(XpandrLink_Tests PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(XpandrLink_Tests PRIVATE
    XpandrLinkBinaryData
    sqlite3_bundled
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
add_test(NAME XpandrLinkTests COMMAND XpandrLink_Tests)

#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <map>
#include <array>
#include "../Engine/MidiEngine.h"
#include "../UI/ModAssignmentLogic.h"
#include "../UI/VoiceArchitectureComponent.h"
#include "../UI/FullModMatrixPanel.h"
#include "../UI/AdvancedParamsPanel.h"
#include "../Data/PatchLibrary.h"
#include "../UI/PatchBrowserPanel.h"
#include "../UI/RandomizerPanel.h"
#include "../UI/MorphPanel.h"
#include "../UI/RndMorphPanel.h"
#include "../UI/CcMapPanel.h"
#include "../UI/MidiSettingsPanel.h"
#include "../UI/HardwareButtonLookAndFeel.h"
#include "../UI/TitleBarComponent.h"
#include "../UI/BottomPaneManager.h"

class EditorTabComponent : public juce::Component, public MidiEngine::Listener
{
public:
    EditorTabComponent(MidiEngine& e, ModAssignmentLogic& modLogic);
    ~EditorTabComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void onMidiActivity(bool isInput) override;
    void onPatchReceived(const std::vector<uint8_t>& rawPatchBytes, int progNum) override;
    void onParameterChangedFromHardware(int page, int paramCol, int value, bool isDelta, bool isButton) override;
    void onStatusUpdate(const juce::String& msg) override;
    void onProgramChangeReceived(int programNumber) override;
    void onMidiOutputChanged(const juce::String& name) override;
    void onSynthInputDetected(const juce::String& portName) override;
    void onSysexIDDetected(int id) override;
    void onModulationRoutingChangedByHardware() override;
    void onPatchSentToSynth(int programNumber) override;

    std::vector<uint8_t> buildCurrentPatchSysex() const;

private:
    // The title bar's VFD number and the hidden programSelector combo box are two
    // separate stores of "current patch number" — prev/next nav reads programSelector,
    // while the VFD display reads titleBar_. Every code path that changes the current
    // patch number must go through here so the two never drift apart (they did: loading
    // a specific patch by number, then by hardware/library, left programSelector stale,
    // so the next ◀/▶ click jumped from whatever programSelector last held).
    void setCurrentProgramNumber(int progNum);

    // ---- Patch assembly (shared by save and randomizer) --------------------
    // Current {paramID -> value} from lastRawPatch overlaid with live UI values.
    std::map<int,int> currentParamMap() const;
    // Pack a param map + 60-byte mod region + name into a 399-byte single-patch SysEx.
    std::vector<uint8_t> encodePatchSysex(const std::map<int,int>& params,
                                          const uint8_t* modBytes,
                                          const juce::String& name) const;

    // ---- Randomizer (TASK-0) -----------------------------------------------
    void doNudge();
    void doRandomize();
    void doRevert();
    void applyRandomizedPatch(const std::map<int,int>& params,
                              const std::array<uint8_t,60>& modBytes);
    std::array<uint8_t,60> currentModBytes() const;
    std::vector<uint8_t> revertSysex_;   // one-level undo snapshot (pre-roll patch)

    MidiEngine&         midiEngine;
    ModAssignmentLogic& modAssignmentLogic;
    std::unique_ptr<juce::PropertiesFile> appProperties;

    // Declared before the buttons so it outlives them (members destruct in reverse order).
    HardwareButtonLookAndFeel hwButtonLF_;

    PatchLibrary patchLibrary_;
    std::unique_ptr<PatchBrowserPanel> patchBrowserPanel_;
    void openOrClosePatchBrowser();
    void openMorphBBrowser();
    void loadPatchFromHardware(int progNum);  // TASK-05: inline-edit patch number → load from hardware
    void confirmThenRun(const juce::String& title, const juce::String& message,
                        const juce::String& okText, std::function<void()> onConfirm);
    void initPatchWithConfirm();
    void tuneAllWithConfirm();

    void loadSettings();
    void saveSettings();

    // MIDI controls (hidden, kept for settings persistence)
    juce::ComboBox midiInputSelector, midiOutputSelector, idSelector, programSelector;
    juce::TextButton clearLogButton, getPatchButton{"Get Patch"};

    std::unique_ptr<TitleBarComponent> titleBar_;

    juce::String currentPatchName{"--------"};
    std::vector<uint8_t> lastRawPatch;

    // Main content pages
    std::unique_ptr<VoiceArchitectureComponent> voiceArch;
    std::unique_ptr<FullModMatrixPanel>  fullModMatrix;
    std::unique_ptr<AdvancedParamsPanel> advancedPanel;
    std::unique_ptr<RandomizerPanel>     randomizerPanel_;
    std::unique_ptr<MorphPanel>          morphPanel_;
    std::unique_ptr<RndMorphPanel>       rndMorphPanel_;
    std::unique_ptr<PatchBrowserPanel>   morphBBrowserPanel_;
    std::unique_ptr<CcMapPanel>          ccMapPanel_;
    std::unique_ptr<MidiSettingsPanel>   midiSettingsPanel_;

    // Bottom pane toggle buttons (nav bar)
    juce::TextButton btnMod{"Mod"}, btnAdv{"PAGE2"}, btnRndMorph{"RND/MORPH"}, btnCc{"CC"}, btnMidi{"MIDI"};
    juce::TextButton btnInit_{"INIT PATCH"}, btnMute_{"MUTE"}, btnTuneAll_{"TUNE ALL"};
    BottomPaneManager bottomPaneManager_;

    juce::TextEditor logEditor;
    juce::StringArray lastLogLines;   // ring buffer — capped at kLogCap entries
    static constexpr int kLogCap = 200;

    // Pre-built per-page param lookup — replaces O(350) linear scan in onParameterChangedFromHardware.
    std::unordered_map<int, std::vector<const XpanderParam*>> paramsByPage_;

    // Debounce counter for hardware mod-routing dump requests.
    // Incremented on each MIDI-thread notification; only the final pending dump fires.
    std::atomic<int> modDumpGeneration_ { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorTabComponent)
};

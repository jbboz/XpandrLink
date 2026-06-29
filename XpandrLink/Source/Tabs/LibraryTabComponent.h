/*
  ==============================================================================
    LibraryTabComponent.h
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "../UI/PatchBrowser.h"

class LibraryTabComponent : public juce::Component,
                             public MidiEngine::Listener
{
public:
    LibraryTabComponent(MidiEngine& e);
    ~LibraryTabComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // MidiEngine::Listener
    void onMidiActivity(bool) override {}
    void onPatchReceived(const std::vector<uint8_t>&, int) override;
    void onParameterChangedFromHardware(int, int, int, bool, bool) override {}
    void onStatusUpdate(const juce::String&) override {}

    // Set by MainComponent. Called at the start of Save so the file reflects
    // the current editor state rather than the stale load-time cache.
    std::function<void()> onBeforeSave;

    // Called when the user clicks the "← Editor" button to return to the editor tab.
    std::function<void()> onEditorRequested;

private:
    MidiEngine& midiEngine;

    PatchBrowser patchBrowser;

    juce::TextButton btnEditor        { "Editor" };
    juce::TextButton btnLoad         { "Load to Editor" };
    juce::TextButton btnSave         { "Save Patch" };
    juce::TextButton btnChooseFolder { "Choose Folder" };
    juce::TextButton btnRefresh      { "Refresh" };
    juce::TextButton btnUp           { "\xe2\x86\x91" };   // ↑ navigate to parent folder
    juce::TextButton btnAutoLoad     { "Auto-load" };

    juce::Label statusLabel;
    juce::Label folderLabel;   // shows current browse path

    juce::File selectedFile;
    bool autoLoadEnabled = false;

    std::unique_ptr<juce::PropertiesFile> appProperties;

    void loadSettings();
    void saveSettings();
    void updateButtonState();
    void updateFolderLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryTabComponent)
};

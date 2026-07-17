/*
  ==============================================================================
    LibraryTabComponent.cpp
  ==============================================================================
*/

#include "LibraryTabComponent.h"

LibraryTabComponent::LibraryTabComponent(MidiEngine& e) : midiEngine(e)
{
    // Settings
    try {
        juce::PropertiesFile::Options opts;
        opts.applicationName    = "XpandrLink";
        opts.filenameSuffix     = ".settings";
        opts.osxLibrarySubFolder = "Application Support";
        opts.folderName         = "XpandrLink";
        opts.storageFormat      = juce::PropertiesFile::storeAsXML;
        appProperties.reset(new juce::PropertiesFile(opts));
    } catch (...) {}

    addAndMakeVisible(patchBrowser);
    addAndMakeVisible(btnEditor);
    addAndMakeVisible(btnLoad);
    addAndMakeVisible(btnSave);
    addAndMakeVisible(btnChooseFolder);
    addAndMakeVisible(btnRefresh);
    addAndMakeVisible(btnUp);
    addAndMakeVisible(btnAutoLoad);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(folderLabel);

    btnEditor.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff1a2a3a));
    btnEditor.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff55afd2));
    btnEditor.onClick = [this] { if (onEditorRequested) onEditorRequested(); };

    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    statusLabel.setText("No patch selected", juce::dontSendNotification);

    folderLabel.setColour(juce::Label::textColourId, juce::Colour(0xff0099cc));
    folderLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    folderLabel.setJustificationType(juce::Justification::centredRight);

    // Restore saved folder (falls back to ~/Documents/OberheimPatches if not saved)
    loadSettings();

    // Navigate up to parent folder
    btnUp.onClick = [this] {
        patchBrowser.navigateUp();
        updateButtonState();
        updateFolderLabel();
    };

    // Auto-load toggle
    btnAutoLoad.setClickingTogglesState(true);
    btnAutoLoad.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2c5fbe));
    btnAutoLoad.onClick = [this] {
        autoLoadEnabled = btnAutoLoad.getToggleState();
    };

    // Directory change — keep Up button and folder label in sync
    patchBrowser.onRootChanged = [this] {
        updateFolderLabel();
        btnUp.setEnabled(patchBrowser.canNavigateUp());
    };

    // File selection — show validity cue; auto-load if enabled
    patchBrowser.onPatchSelected = [this](const juce::File& f) {
        selectedFile = f;
        if (f.getSize() < 399) {
            statusLabel.setText("Not a valid Xpander patch — " + juce::String(f.getSize()) + " bytes",
                                juce::dontSendNotification);
        } else {
            statusLabel.setText(f.getFileNameWithoutExtension(), juce::dontSendNotification);
        }
        updateButtonState();
        if (autoLoadEnabled && f.getSize() >= 399) btnLoad.triggerClick();
    };

    // Load selected patch into editor and send to synth
    btnLoad.onClick = [this] {
        if (!selectedFile.existsAsFile()) return;
        bool ok = midiEngine.loadPatchFromFile(selectedFile);
        if (ok) {
            // Send the loaded patch to the hardware so it is audible immediately.
            // If no MIDI output is connected this is a silent no-op.
            midiEngine.sendPatchToSynth();
            statusLabel.setText("Loaded + sent: " + selectedFile.getFileNameWithoutExtension(),
                                juce::dontSendNotification);
        } else {
            statusLabel.setText("Error: not a valid Xpander single patch",
                                juce::dontSendNotification);
        }
        updateButtonState();
    };

    // Save current cached patch to a file
    btnSave.onClick = [this] {
        // Encode live UI state into the cache before writing to disk
        if (onBeforeSave) onBeforeSave();

        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Patch As",
            patchBrowser.getRootDirectory().getChildFile("patch.syx"),
            "*.syx");
        juce::Component::SafePointer<LibraryTabComponent> safeThis(this);
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [safeThis, chooser](const juce::FileChooser& fc) {
                if (safeThis == nullptr) return;
                juce::File result = fc.getResult();
                if (result == juce::File{}) return;
                juce::File dest = result.withFileExtension("syx");
                bool ok = safeThis->midiEngine.savePatchToFile(dest);
                safeThis->statusLabel.setText(
                    ok ? "Saved: " + dest.getFileName() : "Save failed — use Get Patch first",
                    juce::dontSendNotification);
                if (ok) safeThis->patchBrowser.refresh();
            });
    };

    // Let the user navigate to any folder containing .syx patches
    btnChooseFolder.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Choose Patch Folder",
            patchBrowser.getRootDirectory());
        juce::Component::SafePointer<LibraryTabComponent> safeThis(this);
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [safeThis, chooser](const juce::FileChooser& fc) {
                if (safeThis == nullptr) return;
                juce::File result = fc.getResult();
                if (!result.isDirectory()) return;
                safeThis->patchBrowser.setRootDirectory(result);
                safeThis->selectedFile = juce::File{};   // clear stale selection
                safeThis->updateFolderLabel();
                safeThis->updateButtonState();
                safeThis->saveSettings();
            });
    };

    btnRefresh.onClick = [this] {
        patchBrowser.refresh();
        statusLabel.setText("Refreshed", juce::dontSendNotification);
    };

    midiEngine.addListener(this);
    updateButtonState();
    updateFolderLabel();
    btnUp.setEnabled(patchBrowser.canNavigateUp());
}

LibraryTabComponent::~LibraryTabComponent()
{
    midiEngine.removeListener(this);
}

// ---- Settings ---------------------------------------------------------------

void LibraryTabComponent::loadSettings()
{
    if (!appProperties) return;
    juce::String saved = appProperties->getValue("LibraryFolder");
    if (saved.isNotEmpty()) {
        juce::File dir(saved);
        if (dir.isDirectory())
            patchBrowser.setRootDirectory(dir);
    }
}

void LibraryTabComponent::saveSettings()
{
    if (!appProperties) return;
    appProperties->setValue("LibraryFolder",
                            patchBrowser.getRootDirectory().getFullPathName());
    appProperties->save();
}

// ---- MidiEngine::Listener ---------------------------------------------------

void LibraryTabComponent::onPatchReceived(const std::vector<uint8_t>&, int)
{
    // Save is always enabled now (encodes live state on demand), nothing to update.
}

// ---- Helpers ----------------------------------------------------------------

void LibraryTabComponent::updateButtonState()
{
    btnLoad.setEnabled(selectedFile.existsAsFile());
    btnSave.setEnabled(true);  // always available — onBeforeSave encodes live UI state on demand
}

void LibraryTabComponent::updateFolderLabel()
{
    folderLabel.setText(patchBrowser.getRootDirectory().getFullPathName(),
                        juce::dontSendNotification);
}

// ---- Drawing / layout -------------------------------------------------------

void LibraryTabComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));

    g.setColour(juce::Colour(0xff111111));
    g.fillRect(0, 0, getWidth(), 40);

    g.setColour(juce::Colour(0xff004466));
    g.drawHorizontalLine(40, 0.0f, (float)getWidth());
}

void LibraryTabComponent::resized()
{
    auto area = getLocalBounds();

    // Toolbar row
    auto toolbar = area.removeFromTop(40).reduced(5, 6);
    btnEditor.setBounds(toolbar.removeFromLeft(80));        toolbar.removeFromLeft(8);
    btnLoad.setBounds(toolbar.removeFromLeft(100));         toolbar.removeFromLeft(4);
    btnSave.setBounds(toolbar.removeFromLeft(80));          toolbar.removeFromLeft(4);
    btnChooseFolder.setBounds(toolbar.removeFromLeft(90));  toolbar.removeFromLeft(4);
    btnRefresh.setBounds(toolbar.removeFromLeft(60));       toolbar.removeFromLeft(4);
    btnUp.setBounds(toolbar.removeFromLeft(28));            toolbar.removeFromLeft(4);
    btnAutoLoad.setBounds(toolbar.removeFromLeft(72));

    // Status bar (bottom) — left: status text, right: folder path
    auto statusBar = area.removeFromBottom(22);
    statusLabel.setBounds(statusBar.removeFromLeft(statusBar.getWidth() / 2).reduced(6, 2));
    folderLabel.setBounds(statusBar.reduced(6, 2));

    // File browser fills remaining space
    patchBrowser.setBounds(area.reduced(4));
}

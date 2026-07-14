/*
  PatchBrowserPanel.h
  Floating dropdown patch library browser.
  Opened by clicking the VFD patch-name area in the title bar.
  (Split into .h/.cpp — TASK-13a)
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"
#include "../Data/PatchLibrary.h"
#include "../Engine/MidiEngine.h"

class PatchBrowserPanel : public juce::Component,
                          public juce::ListBoxModel
{
public:
    // ---- Callbacks set by EditorTabComponent --------------------------------
    std::function<void()>                         onClose;
    std::function<void(const PatchEntry&, int)>   onPatchLoaded;
    std::function<void(const juce::String&)>      onPatchRenamed;
    // Returns freshly-encoded current editor SysEx (also sets MidiEngine cache)
    std::function<std::vector<uint8_t>()>         getCurrentSysex;
    std::function<juce::String()>                 getCurrentPatchName;

    PatchBrowserPanel(PatchLibrary& library, MidiEngine& engine);
    ~PatchBrowserPanel() override;

    // Reload from DB and repaint
    void refresh();

    // When true, selecting a patch only fires onPatchLoaded with the entry — it does
    // NOT load the patch into the editor/hardware (which would broadcast onPatchReceived
    // and clobber unrelated state, e.g. Morph's Patch A). Used by the Morph "Load B"
    // browser. Default false = normal load-into-editor behaviour.
    void setSelectOnlyMode(bool b) { selectOnly_ = b; }

    // ---- Component overrides ------------------------------------------------
    void visibilityChanged() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ---- ListBoxModel -------------------------------------------------------
    int getNumRows() override { return (int)filteredIndices_.size(); }
    void selectedRowsChanged(int lastRowSelected) override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool isSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
    void backgroundClicked(const juce::MouseEvent&) override;

    // ---- Desktop::MouseListener — click-outside dismissal ------------------
    void mouseDown(const juce::MouseEvent& e) override;

private:
    static constexpr int kHeaderH = 36;
    static constexpr int kFooterH = 36;
    static constexpr int kStatusH = 36;  // two rows: patch count + library folder path

    PatchLibrary& library_;
    MidiEngine&   midiEngine_;

    juce::TextButton  btnPrev_, btnNext_;
    juce::TextEditor  searchBox_;
    juce::TextButton  btnAll_, btnFav_, btnSort_;
    juce::ListBox     listBox_;
    juce::TextButton  btnImport_, btnSave_, btnSaveAs_, btnRename_, btnRemove_, btnRemoveDupes_, btnStore_;
    juce::Label       statusLabel_;
    juce::TextButton  btnFolder_;
    juce::Label       folderLabel_;

    juce::Array<int>  filteredIndices_;
    bool              showFavoritesOnly_ = false;
    int               selectedRow_       = -1;
    int               loadGeneration_    = 0;   // cancels stale debounced loads
    bool              selectOnly_        = false; // report-only selection (Morph Load B)
    juce::int64       showedAt_          = 0;   // time panel became visible (ms)

    std::unique_ptr<juce::FileChooser> fileChooser_;
    std::unique_ptr<juce::FileChooser> folderChooser_;

    // ---- Helpers ------------------------------------------------------------
    void setupNavBtn(juce::TextButton& btn, const juce::String& text);
    void setupFilterBtn(juce::TextButton& btn, const juce::String& text);
    void setupActionBtn(juce::TextButton& btn, const juce::String& text);

    // Step to the previous (-1) or next (+1) patch in the filtered list and load it.
    void stepBy(int delta);

    void applyFilter();
    void updateActionButtons();
    void loadPatch(int libIdx);

    // ---- Footer button actions ----------------------------------------------
    // Single Import handles both single-patch and bank (.syx) files — the library
    // auto-detects how many patches each file contains.
    void doImport();
    void importNextWithPrompt(juce::Array<juce::File> files, int index, int importedTotal);
    void doSaveCurrent();
    void doSaveAs();
    void doRename();
    void doRemove();
    void doStore();
    void cycleSort();
    void updateSortButtonLabel();
    void doEditDescription(int row);
    void updateFolderLabel();
    void doChooseFolder();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchBrowserPanel)
};

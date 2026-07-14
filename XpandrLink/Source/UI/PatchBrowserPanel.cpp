/*
  PatchBrowserPanel.cpp
  Implementations split out from PatchBrowserPanel.h (TASK-13a).
*/
#include "PatchBrowserPanel.h"

PatchBrowserPanel::PatchBrowserPanel(PatchLibrary& library, MidiEngine& engine)
    : library_(library), midiEngine_(engine)
{
    auto theme = ThemeData::getHardwareTheme();

    // Prev / Next navigation buttons
    setupNavBtn(btnPrev_, "<");
    setupNavBtn(btnNext_, ">");
    btnPrev_.onClick = [this] { stepBy(-1); };
    btnNext_.onClick = [this] { stepBy(1); };

    // Search box
    searchBox_.setTextToShowWhenEmpty("Search patches...",
                                      juce::Colour(0xff606060));
    searchBox_.setFont(juce::Font(juce::FontOptions(12.0f)));
    searchBox_.setColour(juce::TextEditor::backgroundColourId,  juce::Colour(0xff2a2a2a));
    searchBox_.setColour(juce::TextEditor::textColourId,         juce::Colour(0xffcccccc));
    searchBox_.setColour(juce::TextEditor::outlineColourId,      juce::Colour(0xff3a3a3a));
    searchBox_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff55afd2));
    searchBox_.onTextChange = [this] { applyFilter(); };
    searchBox_.onEscapeKey  = [this] { if (onClose) onClose(); };
    addAndMakeVisible(searchBox_);

    // Filter buttons — [All] and [★]
    setupFilterBtn(btnAll_,  "ALL");
    setupFilterBtn(btnFav_,  juce::String::fromUTF8("\xe2\x98\x85")); // ★
    btnAll_.setToggleState(true,  juce::dontSendNotification);
    btnFav_.setToggleState(false, juce::dontSendNotification);
    btnAll_.onClick = [this] {
        showFavoritesOnly_ = false;
        btnAll_.setToggleState(true,  juce::dontSendNotification);
        btnFav_.setToggleState(false, juce::dontSendNotification);
        applyFilter();
    };
    btnFav_.onClick = [this] {
        showFavoritesOnly_ = true;
        btnAll_.setToggleState(false, juce::dontSendNotification);
        btnFav_.setToggleState(true,  juce::dontSendNotification);
        applyFilter();
    };

    // Sort cycle button — Name → Date → Description → Name
    setupFilterBtn(btnSort_, "Name");
    btnSort_.onClick = [this] { cycleSort(); };
    updateSortButtonLabel();

    // List box — keyboard navigation built-in; selectedRowsChanged hooks auto-load
    listBox_.setModel(this);
    listBox_.setRowHeight(28);
    listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff242424));
    listBox_.setColour(juce::ListBox::outlineColourId,    juce::Colour(0xff3a3a3a));
    listBox_.setOutlineThickness(0);
    listBox_.setMultipleSelectionEnabled(true);   // cmd/shift-click to multi-select for removal
    listBox_.setWantsKeyboardFocus(true);
    addAndMakeVisible(listBox_);

    // Footer action buttons (single Import auto-detects single vs. bank files)
    setupActionBtn(btnImport_,     "Import");
    setupActionBtn(btnSave_,       "Save");
    setupActionBtn(btnSaveAs_,     "Save As");
    setupActionBtn(btnRename_,     "Rename");
    setupActionBtn(btnRemove_,     "Remove");
    setupActionBtn(btnRemoveDupes_, "Dedupe");
    setupActionBtn(btnStore_,      "Store");

    btnImport_.onClick      = [this] { doImport(); };
    btnSave_.onClick        = [this] { doSaveCurrent(); };
    btnSaveAs_.onClick      = [this] { doSaveAs(); };
    btnRename_.onClick      = [this] { doRename(); };
    btnRemove_.onClick      = [this] { doRemove(); };
    btnStore_.onClick       = [this] { doStore(); };
    btnRemoveDupes_.onClick = [this] {
        int dupes = 0;
        for (int i = 0; i < library_.getNumPatches(); ++i)
            if (library_.getPatch(i).isDuplicate) ++dupes;
        if (dupes == 0) {
            statusLabel_.setText("No duplicate patches found", juce::dontSendNotification);
            return;
        }
        library_.removeDuplicates();
        selectedRow_ = -1;
        applyFilter();
        statusLabel_.setText("Removed " + juce::String(dupes) + " duplicate"
                             + (dupes != 1 ? "s" : ""), juce::dontSendNotification);
    };

    // Status
    statusLabel_.setColour(juce::Label::textColourId,       juce::Colour(0xff888888));
    statusLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    statusLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel_);

    // Library folder row: small "Folder" button + truncated path label.
    btnFolder_.setButtonText("Folder...");
    btnFolder_.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff2a2a2a));
    btnFolder_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff55afd2));
    btnFolder_.onClick = [this] { doChooseFolder(); };
    addAndMakeVisible(btnFolder_);

    folderLabel_.setColour(juce::Label::textColourId,       juce::Colour(0xff666666));
    folderLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    folderLabel_.setFont(juce::Font(juce::FontOptions(9.5f)));
    folderLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(folderLabel_);
    updateFolderLabel();

    applyFilter();
}

PatchBrowserPanel::~PatchBrowserPanel()
{
    juce::Desktop::getInstance().removeGlobalMouseListener(this);
}

void PatchBrowserPanel::refresh()
{
    library_.refresh();
    applyFilter();
}

void PatchBrowserPanel::visibilityChanged()
{
    if (isVisible())
    {
        showedAt_ = juce::Time::currentTimeMillis();
        juce::Desktop::getInstance().addGlobalMouseListener(this);
        searchBox_.grabKeyboardFocus();
    }
    else
    {
        juce::Desktop::getInstance().removeGlobalMouseListener(this);
    }
}

void PatchBrowserPanel::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();
    auto bounds = getLocalBounds().toFloat();

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(bounds.translated(3.0f, 3.0f), 4.0f);

    // Background
    g.setColour(theme.moduleBackground);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Border
    g.setColour(theme.groupOutline.brighter(0.8f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    // Search / footer separators
    g.setColour(theme.groupOutline);
    g.drawHorizontalLine(kHeaderH, 1.0f, (float)getWidth() - 1.0f);
    g.drawHorizontalLine(getHeight() - kFooterH - kStatusH, 1.0f, (float)getWidth() - 1.0f);
}

void PatchBrowserPanel::resized()
{
    auto r = getLocalBounds().reduced(1);

    // Header row: [◀][▶] [searchBox] [Sort][ALL][★]
    auto header = r.removeFromTop(kHeaderH).reduced(4, 3);
    btnFav_.setBounds(header.removeFromRight(32));
    header.removeFromRight(3);
    btnAll_.setBounds(header.removeFromRight(36));
    header.removeFromRight(3);
    btnSort_.setBounds(header.removeFromRight(46));
    header.removeFromRight(5);
    btnPrev_.setBounds(header.removeFromLeft(28));
    header.removeFromLeft(2);
    btnNext_.setBounds(header.removeFromLeft(28));
    header.removeFromLeft(5);
    searchBox_.setBounds(header);

    // Status area — two rows: patch count + folder path.
    auto status = r.removeFromBottom(kStatusH);
    auto statusTop = status.removeFromTop(kStatusH / 2);
    auto statusBot = status;
    statusLabel_.setBounds(statusTop.reduced(6, 2));
    btnFolder_.setBounds(statusBot.removeFromLeft(68).reduced(4, 2));
    statusBot.removeFromLeft(4);
    folderLabel_.setBounds(statusBot.reduced(0, 2));

    // Footer — seven buttons share the width evenly.
    auto footer = r.removeFromBottom(kFooterH).reduced(4, 3);
    const int nBtns = 7, btnGap = 4;
    int bw = (footer.getWidth() - (nBtns - 1) * btnGap) / nBtns;
    btnImport_.setBounds     (footer.removeFromLeft(bw)); footer.removeFromLeft(btnGap);
    btnSave_.setBounds       (footer.removeFromLeft(bw)); footer.removeFromLeft(btnGap);
    btnSaveAs_.setBounds     (footer.removeFromLeft(bw)); footer.removeFromLeft(btnGap);
    btnRename_.setBounds     (footer.removeFromLeft(bw)); footer.removeFromLeft(btnGap);
    btnRemove_.setBounds     (footer.removeFromLeft(bw)); footer.removeFromLeft(btnGap);
    btnRemoveDupes_.setBounds(footer.removeFromLeft(bw)); footer.removeFromLeft(btnGap);
    btnStore_.setBounds(footer);

    // List fills rest
    listBox_.setBounds(r);
}

// Called by ListBox whenever selection changes (keyboard OR mouse).
// We use this for auto-load: increment a generation counter so only the
// last navigation event within the debounce window actually loads.
void PatchBrowserPanel::selectedRowsChanged(int lastRowSelected)
{
    updateActionButtons();   // keep Remove/Rename enable state in sync with the selection
    if (lastRowSelected < 0 || lastRowSelected >= (int)filteredIndices_.size()) return;
    int gen = ++loadGeneration_;
    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    juce::Timer::callAfterDelay(150, [safe, gen, lastRowSelected] {
        if (!safe || safe->loadGeneration_ != gen) return;
        if (lastRowSelected >= (int)safe->filteredIndices_.size()) return;
        safe->loadPatch(safe->filteredIndices_[lastRowSelected]);
    });
}

void PatchBrowserPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height,
                                         bool isSelected)
{
    if (row < 0 || row >= (int)filteredIndices_.size()) return;
    const auto& e = library_.getPatch(filteredIndices_[row]);
    auto theme = ThemeData::getHardwareTheme();

    // Row background
    if (isSelected)
        g.fillAll(theme.headerBand.withAlpha(0.85f));
    else if (row % 2 == 0)
        g.fillAll(juce::Colour(0xff242424));
    else
        g.fillAll(juce::Colour(0xff202020));

    // Star (left 28px)
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    if (e.favorite)
    {
        g.setColour(theme.ledOn);
        g.drawText(juce::String::fromUTF8("\xe2\x98\x85"), 0, 0, 28, height, juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xff444444));
        g.drawText(juce::String::fromUTF8("\xe2\x98\x86"), 0, 0, 28, height, juce::Justification::centred);
    }

    // Patch name — VFD font; amber tint for duplicates
    g.setFont(ThemeData::getVfdFont(12.0f));
    if (e.isDuplicate)
        g.setColour(isSelected ? juce::Colour(0xfffff8b0) : juce::Colour(0xffb08820));
    else
        g.setColour(isSelected ? juce::Colour(0xffffffe0) : juce::Colour(0xffbfffd7));
    auto nameLabel = e.isDuplicate ? e.name + " [D]" : e.name;
    g.drawText(nameLabel, 32, 0, 148, height, juce::Justification::centredLeft, false);

    // Date added — compact mm/dd/yy at the right edge (TASK-06)
    const int dateW = (e.dateAdded > 0) ? 64 : 0;
    if (dateW > 0)
    {
        juce::Time t(e.dateAdded * 1000);
        g.setFont(juce::Font(juce::FontOptions(10.5f)));
        g.setColour(isSelected ? juce::Colour(0xff99ccdd) : juce::Colour(0xff707f7f));
        g.drawText(t.formatted("%m/%d/%y"), width - dateW - 6, 0, dateW, height,
                   juce::Justification::centredRight, false);
    }

    // Description — readable size, close to the patch-name size
    g.setFont(juce::Font(juce::FontOptions(13.5f)));
    g.setColour(isSelected ? juce::Colour(0xff99ccdd) : juce::Colour(0xff8aa0a0));
    juce::String desc = e.description.isNotEmpty()
                          ? e.description
                          : e.file.getParentDirectory().getFileName();
    g.drawText(desc, 184, 0, width - 188 - (dateW > 0 ? dateW + 8 : 0), height,
               juce::Justification::centredLeft, true);
}

void PatchBrowserPanel::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int)filteredIndices_.size()) return;
    int libIdx = filteredIndices_[row];

    if (e.x < 28)
    {
        // Star zone — toggle favorite only, suppress the selectedRowsChanged auto-load.
        // selectedRowsChanged already fired (on mouse-down) and queued a debounced load;
        // incrementing the generation here cancels that pending load.
        ++loadGeneration_;
        library_.toggleFavorite(libIdx);
        if (showFavoritesOnly_) { applyFilter(); listBox_.updateContent(); }
        else                    { listBox_.repaintRow(row); }
    }
    else
    {
        // Name zone — load immediately; increment generation to cancel the
        // debounced load that selectedRowsChanged already queued.
        selectedRow_ = row;
        loadPatch(libIdx);
        ++loadGeneration_;
    }
}

void PatchBrowserPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int)filteredIndices_.size()) return;
    // Double-click on the description column edits the description; elsewhere loads.
    if (e.x >= 184) { doEditDescription(row); return; }
    loadPatch(filteredIndices_[row]);
    ++loadGeneration_;
}

void PatchBrowserPanel::backgroundClicked(const juce::MouseEvent&)
{
    listBox_.deselectAllRows();
    selectedRow_ = -1;
}

void PatchBrowserPanel::mouseDown(const juce::MouseEvent& e)
{
    if (!isVisible()) return;
    if (getScreenBounds().contains(e.getScreenPosition())) return;
    // Ignore click-outside events that arrive within 200ms of the panel
    // opening — the same mouseDown that opened the panel is re-delivered
    // to the global listener, which would immediately close it.
    if (juce::Time::currentTimeMillis() - showedAt_ < 200) return;

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    juce::Timer::callAfterDelay(50, [safe] {
        if (safe && safe->isVisible() && safe->onClose)
            safe->onClose();
    });
}

// ---- Helpers ------------------------------------------------------------

void PatchBrowserPanel::setupNavBtn(juce::TextButton& btn, const juce::String& text)
{
    btn.setButtonText(text);
    btn.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff2a2a2a));
    btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff55afd2));
    addAndMakeVisible(btn);
}

void PatchBrowserPanel::stepBy(int delta)
{
    if (filteredIndices_.isEmpty()) return;
    int newRow = juce::jlimit(0, (int)filteredIndices_.size() - 1,
                              selectedRow_ + delta);
    if (newRow == selectedRow_ && filteredIndices_.size() > 1) return;
    selectedRow_ = newRow;
    listBox_.selectRow(newRow);
    listBox_.scrollToEnsureRowIsOnscreen(newRow);
    loadPatch(filteredIndices_[newRow]);
    ++loadGeneration_;  // cancel the selectedRowsChanged-triggered debounce
}

void PatchBrowserPanel::setupFilterBtn(juce::TextButton& btn, const juce::String& text)
{
    btn.setButtonText(text);
    btn.setClickingTogglesState(false);
    btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff2a2a2a));
    btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1a4060));
    btn.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff888888));
    btn.setColour(juce::TextButton::textColourOnId,   juce::Colour(0xff55afd2));
    addAndMakeVisible(btn);
}

void PatchBrowserPanel::setupActionBtn(juce::TextButton& btn, const juce::String& text)
{
    btn.setButtonText(text);
    btn.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff222222));
    btn.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff1a4060));
    btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff55afd2));
    addAndMakeVisible(btn);
}

void PatchBrowserPanel::applyFilter()
{
    filteredIndices_ = library_.search(searchBox_.getText(), showFavoritesOnly_);
    // Clamp selectedRow_ to valid range after filter change
    if (selectedRow_ >= (int)filteredIndices_.size())
        selectedRow_ = (int)filteredIndices_.size() - 1;
    listBox_.updateContent();
    if (selectedRow_ >= 0)
        listBox_.selectRow(selectedRow_, false, true);
    listBox_.repaint();
    updateActionButtons();

    // Status line: patch count + duplicate count
    int total = library_.getNumPatches();
    int dupes = 0;
    for (int i = 0; i < total; ++i)
        if (library_.getPatch(i).isDuplicate) ++dupes;
    juce::String status = juce::String(total) + " patch" + (total != 1 ? "es" : "");
    if (dupes > 0) status += " \xc2\xb7 " + juce::String(dupes) + " duplicate" + (dupes != 1 ? "s" : "");
    statusLabel_.setText(status, juce::dontSendNotification);
}

void PatchBrowserPanel::updateActionButtons()
{
    bool hasSelection = (selectedRow_ >= 0 && selectedRow_ < (int)filteredIndices_.size());
    int  numSelected  = listBox_.getSelectedRows().size();
    btnRename_.setEnabled(numSelected == 1 || hasSelection);  // rename targets a single patch
    btnRemove_.setEnabled(numSelected > 0 || hasSelection);   // remove supports multi-select
    btnPrev_.setEnabled(hasSelection && selectedRow_ > 0);
    btnNext_.setEnabled(hasSelection && selectedRow_ < (int)filteredIndices_.size() - 1);
}

void PatchBrowserPanel::loadPatch(int libIdx)
{
    const auto& entry = library_.getPatch(libIdx);

    // Report-only mode (Morph "Load B"): hand the entry to the owner without loading it
    // into the editor/hardware. Loading would broadcast onPatchReceived and overwrite
    // Morph's Patch A with the just-selected B patch.
    if (selectOnly_)
    {
        for (int r = 0; r < (int)filteredIndices_.size(); ++r)
            if (filteredIndices_[r] == libIdx) { selectedRow_ = r; break; }
        statusLabel_.setText("Selected: " + entry.name, juce::dontSendNotification);
        updateActionButtons();
        if (onPatchLoaded) onPatchLoaded(entry, libIdx);
        return;
    }

    bool ok = midiEngine_.loadPatchFromFile(entry.file);
    if (ok)
    {
        midiEngine_.sendPatchToSynth();
        // Update selectedRow_ to match the loaded patch so stepBy() knows position
        for (int r = 0; r < (int)filteredIndices_.size(); ++r)
            if (filteredIndices_[r] == libIdx) { selectedRow_ = r; break; }
        statusLabel_.setText("Loaded: " + entry.name, juce::dontSendNotification);
        updateActionButtons();
        if (onPatchLoaded) onPatchLoaded(entry, libIdx);
    }
    else
    {
        statusLabel_.setText("Failed: " + entry.file.getFileName(),
                             juce::dontSendNotification);
    }
}

// ---- Footer button actions ----------------------------------------------

void PatchBrowserPanel::doImport()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Import Patch or Bank (.syx)",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.syx");
    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe](const juce::FileChooser& fc) {
            if (!safe) return;
            safe->importNextWithPrompt(fc.getResults(), 0, 0);
        });
}

// Import the selected files one at a time, prompting for a description before each
// (default = the file's name). Cancel on a prompt aborts the remaining imports.
void PatchBrowserPanel::importNextWithPrompt(juce::Array<juce::File> files, int index, int importedTotal)
{
    // Skip any non-patch files up front.
    while (index < files.size() && PatchLibrary::patchCountInFile(files[index]) <= 0)
        ++index;

    if (index >= files.size())
    {
        if (importedTotal > 0) {
            applyFilter();
            statusLabel_.setText("Imported " + juce::String(importedTotal) + " patch"
                                 + (importedTotal != 1 ? "es" : ""), juce::dontSendNotification);
        } else {
            statusLabel_.setText("Import failed — no valid patches found", juce::dontSendNotification);
        }
        return;
    }

    juce::File f = files[index];
    int n = PatchLibrary::patchCountInFile(f);
    juce::String title = (n == 1) ? "Import Patch"
                                  : "Import Bank (" + juce::String(n) + " patches)";

    auto* alert = new juce::AlertWindow(title, "Enter a description for this "
                                        + juce::String(n == 1 ? "patch" : "bank") + ":",
                                        juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("desc", f.getFileNameWithoutExtension(), "Description:");
    alert->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, alert, files, index, importedTotal, f](int result) {
            if (!safe) { delete alert; return; }
            if (result == 1) {
                juce::String desc = alert->getTextEditorContents("desc").trim();
                int added = safe->library_.importFile(f, desc);
                delete alert;
                safe->importNextWithPrompt(files, index + 1, importedTotal + added);
            } else {
                delete alert;   // Cancel → stop here, report what was imported so far
                safe->importNextWithPrompt(files, files.size(), importedTotal);
            }
        }), true);
}

void PatchBrowserPanel::doSaveCurrent()
{
    if (!getCurrentSysex) return;
    auto sysex = getCurrentSysex();
    juce::String name = getCurrentPatchName ? getCurrentPatchName() : "UNTITLED";
    int idx = library_.saveCurrentPatch(sysex, name);
    if (idx >= 0) {
        applyFilter();
        statusLabel_.setText("Saved: " + library_.getPatch(idx).name,
                             juce::dontSendNotification);
    } else {
        statusLabel_.setText("Save failed — use Get Patch first",
                             juce::dontSendNotification);
    }
}

// Save the current editor patch as a NEW library entry under a chosen name, so the
// user can fork a patch and edit the copy without touching the original.
void PatchBrowserPanel::doSaveAs()
{
    if (!getCurrentSysex) return;
    juce::String cur = getCurrentPatchName ? getCurrentPatchName() : "UNTITLED";
    juce::String suggested = (cur + " 2").substring(0, 8).trim();

    auto* alert = new juce::AlertWindow("Save As",
                                        "Save the current patch as a new copy (max 8 chars):",
                                        juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("name", suggested, "Name:");
    alert->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, alert](int result) {
            if (!safe) { delete alert; return; }
            if (result == 1 && safe->getCurrentSysex) {
                juce::String newName = alert->getTextEditorContents("name").toUpperCase().substring(0, 8).trim();
                if (newName.isNotEmpty()) {
                    auto sysex = safe->getCurrentSysex();
                    int idx = safe->library_.saveCurrentPatch(sysex, newName);
                    if (idx >= 0) {
                        safe->applyFilter();
                        // Adopt the new name in the editor — the patch buffer is now this copy.
                        if (safe->onPatchRenamed) safe->onPatchRenamed(newName);
                        safe->statusLabel_.setText("Saved copy: " + safe->library_.getPatch(idx).name,
                                                   juce::dontSendNotification);
                    } else {
                        safe->statusLabel_.setText("Save As failed", juce::dontSendNotification);
                    }
                }
            }
            delete alert;
        }), true);
}

void PatchBrowserPanel::doRename()
{
    if (selectedRow_ < 0 || selectedRow_ >= (int)filteredIndices_.size()) return;
    int libIdx = filteredIndices_[selectedRow_];
    const auto& entry = library_.getPatch(libIdx);

    auto* alert = new juce::AlertWindow("Rename Patch", "Enter new patch name (max 8 chars):",
                                        juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("name", entry.name, "Name:");
    alert->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, libIdx, alert](int result) {
            if (!safe) return;
            if (result == 1) {
                juce::String newName =
                    alert->getTextEditorContents("name").toUpperCase().substring(0, 8);
                if (newName.isNotEmpty()) {
                    safe->library_.renamePatch(libIdx, newName);
                    safe->applyFilter();
                    safe->statusLabel_.setText("Renamed to: " + newName,
                                               juce::dontSendNotification);
                    if (safe->onPatchRenamed) safe->onPatchRenamed(newName);
                }
            }
            delete alert;
        }), true);
}

void PatchBrowserPanel::doRemove()
{
    // Collect every selected row (multi-select); fall back to the single tracked row.
    juce::Array<int> libIdxs;
    auto rows = listBox_.getSelectedRows();
    for (int i = 0; i < rows.size(); ++i)
    {
        int row = rows[i];
        if (row >= 0 && row < (int)filteredIndices_.size())
            libIdxs.add(filteredIndices_[row]);
    }
    if (libIdxs.isEmpty() && selectedRow_ >= 0 && selectedRow_ < (int)filteredIndices_.size())
        libIdxs.add(filteredIndices_[selectedRow_]);
    if (libIdxs.isEmpty()) return;

    int n = libIdxs.size();
    juce::String onlyName = (n == 1) ? library_.getPatch(libIdxs[0]).name : juce::String();
    juce::String message = (n == 1)
        ? "Remove \"" + onlyName + "\" from the library?"
        : "Remove " + juce::String(n) + " patches from the library?";

    auto* alert = new juce::AlertWindow("Remove Patch" + juce::String(n > 1 ? "es" : ""),
                                        message, juce::MessageBoxIconType::WarningIcon);
    alert->addButton("Remove", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, libIdxs, n, onlyName, alert](int result) {
            if (!safe) { delete alert; return; }
            if (result == 1)
            {
                safe->library_.removePatches(libIdxs);
                safe->selectedRow_ = -1;
                safe->listBox_.deselectAllRows();
                safe->applyFilter();
                safe->statusLabel_.setText(n == 1 ? "Removed: " + onlyName
                                                  : "Removed " + juce::String(n) + " patches",
                                           juce::dontSendNotification);
            }
            delete alert;
        }), true);
}

// Permanently commits the current editor patch to a real hardware program slot.
// Unlike every other load/save path here (which only ever touches the file-based
// library or the hardware scratchpad, slot 99), this writes non-volatile synth
// memory — the confirm dialog below is the only guard against overwriting a
// patch the user cares about.
void PatchBrowserPanel::doStore()
{
    if (!getCurrentSysex) return;

    auto* alert = new juce::AlertWindow("Store to Hardware Slot",
        "Permanently writes the current patch into synth program memory at the "
        "slot you choose, overwriting whatever is stored there now. This cannot "
        "be undone from XpandrLink.\n\n"
        "Slots 0-98 only - 99 is XpandrLink's scratchpad, used for every "
        "load and audition.",
        juce::MessageBoxIconType::WarningIcon);
    alert->addTextEditor("slot", "0", "Slot (0-98):");
    alert->addButton("Store", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, alert](int result) {
            if (!safe) { delete alert; return; }
            if (result == 1)
            {
                int slot = alert->getTextEditorContents("slot").getIntValue();
                if (slot < 0 || slot > 98)
                {
                    safe->statusLabel_.setText("Store failed - slot must be 0-98",
                                               juce::dontSendNotification);
                }
                else
                {
                    safe->getCurrentSysex();  // refresh MidiEngine's cache from the live editor
                    safe->midiEngine_.storePatchToSlot(slot);
                    safe->statusLabel_.setText("Stored to slot " + juce::String(slot),
                                               juce::dontSendNotification);
                }
            }
            delete alert;
        }), true);
}

void PatchBrowserPanel::cycleSort()
{
    using SM = PatchLibrary::SortMode;
    SM m = library_.getSortMode();
    m = (m == SM::Name)      ? SM::DateAdded
      : (m == SM::DateAdded) ? SM::Description
                             : SM::Name;
    library_.setSortMode(m);
    updateSortButtonLabel();
    selectedRow_ = -1;
    listBox_.deselectAllRows();
    applyFilter();
}

void PatchBrowserPanel::updateSortButtonLabel()
{
    switch (library_.getSortMode())
    {
        case PatchLibrary::SortMode::Name:        btnSort_.setButtonText("Name"); break;
        case PatchLibrary::SortMode::DateAdded:   btnSort_.setButtonText("Date"); break;
        case PatchLibrary::SortMode::Description: btnSort_.setButtonText("Desc"); break;
    }
}

void PatchBrowserPanel::doEditDescription(int row)
{
    if (row < 0 || row >= (int)filteredIndices_.size()) return;
    int libIdx = filteredIndices_[row];
    const auto& entry = library_.getPatch(libIdx);

    auto* alert = new juce::AlertWindow("Edit Description",
                                        "Description for " + entry.name + ":",
                                        juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("desc", entry.description, "Description:");
    alert->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, libIdx, alert](int result) {
            if (!safe) return;
            if (result == 1) {
                safe->library_.setDescription(libIdx, alert->getTextEditorContents("desc"));
                safe->applyFilter();
                safe->statusLabel_.setText("Description updated", juce::dontSendNotification);
            }
            delete alert;
        }), true);
}

void PatchBrowserPanel::updateFolderLabel()
{
    // Show the path relative to the home directory if possible (shorter, more readable).
    juce::File root = library_.getLibraryRoot();
    juce::File home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    juce::String display = root.getFullPathName();
    if (root.isAChildOf(home))
        display = "~" + root.getFullPathName().substring(home.getFullPathName().length());
    folderLabel_.setText(display, juce::dontSendNotification);
}

void PatchBrowserPanel::doChooseFolder()
{
    folderChooser_ = std::make_unique<juce::FileChooser>(
        "Choose Library Folder",
        library_.getLibraryRoot(),
        "");
    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    folderChooser_->launchAsync(
        juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectDirectories,
        [safe](const juce::FileChooser& fc) {
            if (!safe) return;
            auto results = fc.getResults();
            if (results.isEmpty()) return;
            juce::File chosen = results[0];
            if (chosen.isDirectory()) {
                safe->library_.setLibraryRoot(chosen);
                safe->library_.refresh();
                safe->updateFolderLabel();
                safe->applyFilter();
                safe->statusLabel_.setText("Library folder changed", juce::dontSendNotification);
            }
        });
}

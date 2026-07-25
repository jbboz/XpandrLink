/*
  PatchBrowserPanelActions.cpp
  Footer-button action handlers for PatchBrowserPanel, split out of PatchBrowserPanel.cpp
  (Phase 4 / P1 decomposition) -- same class, same header, no interface change. Kept in a
  separate translation unit only because these are the modal-dialog-heavy handlers
  (BUG-23, BUG-28 both lived in this exact area historically).
*/
#include "PatchBrowserPanel.h"

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
    alert->addButton("Next", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, alert](int result) {
            int slot = alert->getTextEditorContents("slot").getIntValue();
            delete alert;
            if (!safe || result != 1) return;

            if (slot < 0 || slot > 98)
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Invalid Slot",
                    "Slot must be 0-98. Slot 99 is XpandrLink's scratchpad and "
                    "cannot be used for a permanent store.");
                safe->statusLabel_.setText("Store cancelled - slot must be 0-98",
                                           juce::dontSendNotification);
                return;
            }

            safe->confirmAndStore(slot);
        }), true);
}

// Second, explicit confirmation step naming the exact target slot — separate from the
// slot-entry dialog above so a user who typed a number and hit Enter/Store still gets a
// distinct "are you sure" prompt before hardware memory is overwritten.
void PatchBrowserPanel::confirmAndStore(int slot)
{
    auto* confirm = new juce::AlertWindow("Confirm Store",
        "Store the current patch to slot " + juce::String(slot) + "?\n\n"
        "This permanently overwrites whatever patch is currently in slot "
        + juce::String(slot) + " on the synth. This cannot be undone.",
        juce::MessageBoxIconType::WarningIcon);
    confirm->addButton("Store", 1, juce::KeyPress(juce::KeyPress::returnKey));
    confirm->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PatchBrowserPanel> safe(this);
    confirm->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, slot, confirm](int result) {
            if (!safe) { delete confirm; return; }
            if (result == 1)
            {
                safe->getCurrentSysex();  // refresh MidiEngine's cache from the live editor
                safe->midiEngine_.storePatchToSlot(slot);
                safe->statusLabel_.setText("Stored to slot " + juce::String(slot),
                                           juce::dontSendNotification);
            }
            delete confirm;
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

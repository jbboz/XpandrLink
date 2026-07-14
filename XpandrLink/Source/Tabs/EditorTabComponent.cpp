#include "EditorTabComponent.h"
#include "../Data/XpanderData.h"
#include "../Data/PatchCodec.h"
#include "../UI/ModAssignmentLogic.h"
#include "../UI/ThemeData.h"

EditorTabComponent::EditorTabComponent(MidiEngine& e, ModAssignmentLogic& modLogic)
    : midiEngine(e), modAssignmentLogic(modLogic)
{
    addAndMakeVisible(midiInputSelector);
    midiInputSelector.addItemList(midiEngine.getMidiInputs(), 1);
    midiInputSelector.onChange = [this] { midiEngine.setMidiInput(midiInputSelector.getText()); saveSettings(); };

    addAndMakeVisible(midiOutputSelector);
    midiOutputSelector.addItemList(midiEngine.getMidiOutputs(), 1);
    midiOutputSelector.onChange = [this] { midiEngine.setMidiOutput(midiOutputSelector.getText()); saveSettings(); };

    addAndMakeVisible(idSelector);
    for (int i = 0; i < 16; ++i) idSelector.addItem(juce::String(i), i + 1);
    idSelector.onChange = [this] { midiEngine.setSysexID(idSelector.getSelectedId() - 1); saveSettings(); };

    addAndMakeVisible(programSelector);
    for (int i = 0; i < 100; ++i) programSelector.addItem(juce::String(i), i + 1);
    programSelector.setSelectedId(1, juce::dontSendNotification);
    programSelector.onChange = [this] {
        saveSettings();
        int progNum = programSelector.getSelectedId() - 1;
        titleBar_->setProgramNumber(progNum);
        midiEngine.sendProgramChange(progNum);
        juce::Component::SafePointer<EditorTabComponent> safeThis(this);
        juce::Timer::callAfterDelay(100, [safeThis, progNum] {
            if (safeThis != nullptr)
                safeThis->midiEngine.requestPatchDump(progNum);
        });
    };

    titleBar_ = std::make_unique<TitleBarComponent>();
    addAndMakeVisible(*titleBar_);
    titleBar_->onPrevClicked = [this] {
        int cur = programSelector.getSelectedId();
        if (cur > 1) programSelector.setSelectedId(cur - 1, juce::sendNotification);
    };
    titleBar_->onNextClicked = [this] {
        int cur = programSelector.getSelectedId();
        int total = programSelector.getNumItems();
        if (cur < total) programSelector.setSelectedId(cur + 1, juce::sendNotification);
    };
    titleBar_->onLibraryClicked    = [this] { openOrClosePatchBrowser(); };
    titleBar_->onPatchNumberEdited = [this](int n) { loadPatchFromHardware(n); };

    // Old controls — hidden; kept for internal use
    clearLogButton.setVisible(false);
    getPatchButton.setVisible(false);
    midiInputSelector.setVisible(false);
    midiOutputSelector.setVisible(false);
    idSelector.setVisible(false);
    programSelector.setVisible(false);
    logEditor.setVisible(false);

    getPatchButton.onClick = [this] {
        int progNum = programSelector.getSelectedId() - 1;
        midiEngine.sendProgramChange(progNum);
        juce::Component::SafePointer<EditorTabComponent> safeThis(this);
        juce::Timer::callAfterDelay(100, [safeThis, progNum] {
            if (safeThis != nullptr)
                safeThis->midiEngine.requestPatchDump(progNum);
        });
    };

    voiceArch.reset(new VoiceArchitectureComponent(&midiEngine, &modAssignmentLogic));
    addAndMakeVisible(voiceArch.get());

    fullModMatrix.reset(new FullModMatrixPanel(&modAssignmentLogic));
    fullModMatrix->setMidiEngine(&midiEngine);
    addChildComponent(fullModMatrix.get()); // hidden until MOD page selected

    advancedPanel.reset(new AdvancedParamsPanel(&midiEngine));
    addChildComponent(advancedPanel.get()); // hidden until PAGE2 selected

    randomizerPanel_.reset(new RandomizerPanel());   // parented by RndMorphPanel below
    randomizerPanel_->onNudge     = [this] { doNudge(); };
    randomizerPanel_->onRandomize = [this] { doRandomize(); };
    randomizerPanel_->onRevert    = [this] { doRevert(); };

    morphPanel_.reset(new MorphPanel());       // parented by RndMorphPanel below
    morphPanel_->onApply = [this](const std::map<int,int>& params,
                                  const std::array<uint8_t,60>& mod,
                                  const juce::String& name) {
        auto sysex = encodePatchSysex(params, mod.data(), name);
        midiEngine.setCachedPatch(sysex);
        midiEngine.broadcastCachedPatch();
        midiEngine.sendPatchToSynth();
    };
    morphPanel_->onLoadBRequested = [this] { openMorphBBrowser(); };

    rndMorphPanel_.reset(new RndMorphPanel(*randomizerPanel_, *morphPanel_));
    addChildComponent(rndMorphPanel_.get());   // hidden until RND/MORPH selected

    const bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();

    if (isStandalone)
    {
        ccMapPanel_.reset(new CcMapPanel(midiEngine));
        addChildComponent(ccMapPanel_.get());
        ccMapPanel_->onChanged = [this] { saveSettings(); };
    }

    // MidiSettingsPanel (SYNTH ID dropdown) is created in ALL builds so the ID is
    // manually visible/verifiable in standalone too, not just plugin builds.
    midiSettingsPanel_.reset(new MidiSettingsPanel(midiEngine));
    addChildComponent(midiSettingsPanel_.get());
    midiSettingsPanel_->onChanged = [this] { saveSettings(); };

    // Styling only — BottomPaneManager (addPane/addAction) owns each button's onClick.
    auto styleNavBtn = [&](juce::TextButton& btn) {
        addAndMakeVisible(btn);
        btn.setClickingTogglesState(false);
        btn.setLookAndFeel(&hwButtonLF_);
    };
    styleNavBtn(btnMod);
    styleNavBtn(btnAdv);
    styleNavBtn(btnRndMorph);
    styleNavBtn(btnInit_);
    if (isStandalone) styleNavBtn(btnCc);
    styleNavBtn(btnMidi);
    styleNavBtn(btnMute_);
    styleNavBtn(btnTuneAll_);

    voiceArch->setVisible(true);

    // Register in left→right order. Panes toggle a panel; actions fire a command.
    // (fullModMatrix is populated by onPatchReceived, not here — see history.)
    bottomPaneManager_.onLayoutChanged = [this] { resized(); };
    bottomPaneManager_.addPane(&btnMod,      fullModMatrix.get());
    bottomPaneManager_.addPane(&btnAdv,      advancedPanel.get());
    bottomPaneManager_.addPane(&btnRndMorph, rndMorphPanel_.get());
    bottomPaneManager_.addAction(&btnInit_, [this] { initPatchWithConfirm(); });
    if (isStandalone)
        bottomPaneManager_.addPane(&btnCc, ccMapPanel_.get());
    bottomPaneManager_.addAction(&btnTuneAll_, [this] { tuneAllWithConfirm(); });
    bottomPaneManager_.addPane(&btnMidi, midiSettingsPanel_.get(),
        [this] { if (midiSettingsPanel_) midiSettingsPanel_->refresh(); });
    bottomPaneManager_.addAction(&btnMute_,    [this] { midiEngine.sendMidiMute(); });

    addChildComponent(logEditor);  // hidden — debug log kept for internal use
    logEditor.setMultiLine(true, true);
    logEditor.setReadOnly(true);
    logEditor.setScrollbarsShown(true);
    logEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff000000));
    logEditor.setColour(juce::TextEditor::textColourId, juce::Colours::yellow);

    try {
        juce::PropertiesFile::Options options;
        options.applicationName = "XpandrLink";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName = "XpandrLink";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        appProperties.reset(new juce::PropertiesFile(options));
    } catch (...) {}

    patchLibrary_.open();
    if (!patchLibrary_.isOpen())
        juce::MessageManager::callAsync([] {
            juce::NativeMessageBox::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Library Unavailable",
                "The patch library database could not be opened.\n\n"
                "Check that the XpandrLink application data folder is writable.",
                nullptr);
        });

    // One-time notice: every load/audition/randomize/morph is redirected to hardware
    // program slot 99 (see MidiEngine::sendPatchToSynth) so slots 0-98 are never
    // touched — but slot 99 itself is a real, permanent overwrite each time.
    if (appProperties && !appProperties->getBoolValue("ScratchpadNoticeShown", false)) {
        appProperties->setValue("ScratchpadNoticeShown", true);
        appProperties->save();
        juce::MessageManager::callAsync([] {
            juce::NativeMessageBox::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Patch Scratchpad - Slot 99",
                "XpandrLink uses hardware program slot 99 as a scratchpad for "
                "every patch you load, audition, randomize, or morph.\n\n"
                "Slot 99 is overwritten each time this happens. Your other 99 "
                "program slots (0-98) are never touched during normal editing.\n\n"
                "If you have a patch you want to keep in slot 99, move or save "
                "it before using XpandrLink.",
                nullptr);
        });
    }

    // addListener before loadSettings() so that the "Connected Input/Output" status
    // messages from setMidiInput/setMidiOutput are visible in the log panel.
    midiEngine.addListener(this);

    // Build per-page lookup so onParameterChangedFromHardware avoids O(350) linear scan.
    for (const auto& def : XpanderParams)
        paramsByPage_[def.page].push_back(&def);

    // If a patch was already loaded in this session, populate the UI immediately
    // (covers the case of closing and reopening the plugin editor window).
    midiEngine.broadcastCachedPatch();

    loadSettings();

    // After a routing assignment, immediately update the mod-matrix panels from the
    // known src/dest/amount — don't wait for the patch dump round-trip, since the
    // hardware's dump returns stored memory which won't include the live edit.
    modAssignmentLogic.onModulationAssigned = [this](int srcIdx, int destIdx, int amount) {
        juce::Component::SafePointer<EditorTabComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis, srcIdx, destIdx, amount] {
            if (safeThis == nullptr) return;
            // Compute the next available idSource from current panel state BEFORE adding.
            int nextIdSrc = safeThis->voiceArch->getNextIdSourceForDest(destIdx);
            safeThis->midiEngine.addModulation(srcIdx, destIdx, amount, nextIdSrc);
            safeThis->voiceArch->addModulationEntry(srcIdx, destIdx, amount, nextIdSrc);
            safeThis->fullModMatrix->addEntry(srcIdx, destIdx, amount, nextIdSrc);
        });
    };

    // Right-click remove: look up the stored idSource, send SysEx to zero the slot,
    // decrement higher slots for the same dest, then clear both panels and the LED.
    auto removeModCb = [this](int srcIdx, int destIdx) {
        int idSrc = voiceArch->getIdSourceForEntry(srcIdx, destIdx);
        midiEngine.removeModulation(srcIdx, destIdx, idSrc >= 0 ? idSrc : 0);
        voiceArch->removeModulationEntry(srcIdx, destIdx);
        fullModMatrix->removeEntry(srcIdx, destIdx);
        if (idSrc >= 0) {
            voiceArch->decrementIdSourceAfterRemove(destIdx, idSrc);
            fullModMatrix->decrementIdSourceAfterRemove(destIdx, idSrc);
        }
        int paramID = ModAssignmentLogic::getParamIDForDestination(destIdx);
        if (paramID >= 0)
            modAssignmentLogic.removeRoute(paramID);
        if (auto* top = getTopLevelComponent()) top->repaint();
    };
    voiceArch->setOnRemoveModulation(removeModCb);
    fullModMatrix->onRemoveRequested = removeModCb;

    // When an amount changes in one panel, sync the other (display-only; SysEx already sent).
    fullModMatrix->onAmountChanged = [this](int srcIdx, int destIdx, int newAmt) {
        voiceArch->addModulationEntry(srcIdx, destIdx, newAmt);
    };
    voiceArch->setOnAmountChanged([this](int srcIdx, int destIdx, int newAmt) {
        fullModMatrix->addEntry(srcIdx, destIdx, newAmt);
    });

    // When user picks a new destination in the mod matrix, reroute: remove old + add new.
    // Destination change FROM fullModMatrix: slot is already updated; sync modSummary and
    // update the slot's idSource after hardware confirms the new assignment.
    fullModMatrix->onDestinationChanged = [this](int srcIdx, int oldDstIdx, int newDstIdx, int amount, int idSource) {
        juce::Component::SafePointer<EditorTabComponent> safeThis(this);
        int newIdSrc = voiceArch->getNextIdSourceForDest(newDstIdx);
        voiceArch->removeModulationEntry(srcIdx, oldDstIdx);
        midiEngine.removeModulation(srcIdx, oldDstIdx, idSource);
        // Any OTHER entries left behind at oldDstIdx with a higher slot number get
        // renumbered by the hardware after this delete -- our client-side bookkeeping
        // for those entries must follow, or a future edit to one of them will send
        // SETSIGN/SETUNSIGNEDVALUE/DELETESOURCE to the wrong slot (real hardware-lockup
        // bug, found 2026-07-12 -- see CLAUDE-MEMORY.md). Mirrors removeModCb below.
        if (idSource >= 0)
        {
            voiceArch->decrementIdSourceAfterRemove(oldDstIdx, idSource);
            fullModMatrix->decrementIdSourceAfterRemove(oldDstIdx, idSource);
        }
        juce::Timer::callAfterDelay(250, [safeThis, srcIdx, newDstIdx, amount, newIdSrc]() {
            if (!safeThis) return;
            safeThis->midiEngine.addModulation(srcIdx, newDstIdx, amount, newIdSrc);
            safeThis->voiceArch->addModulationEntry(srcIdx, newDstIdx, amount, newIdSrc);
            // Sync the idSource stored in the fullModMatrix slot for future remove ops
            safeThis->fullModMatrix->updateEntryDestination(srcIdx, newDstIdx, newDstIdx, newIdSrc);
        });
    };

    // When user selects a new source for an EXISTING entry, swap it in place via the
    // atomic CHANGESOURCE command (cmd=0x02) -- never delete+re-add. Delete+re-add
    // requires predicting which hardware slot the re-add will auto-append to, which
    // silently corrupts a DIFFERENT entry's sign/amount when the prediction is wrong
    // (real hardware-lockup bug, found 2026-07-12 -- see CLAUDE-MEMORY.md). CHANGESOURCE
    // keeps the entry's idSource untouched, so there is nothing to mispredict.
    // Source change FROM fullModMatrix: modSummary still has oldSrcIdx → remove+add locally.
    fullModMatrix->onSourceChanged = [this](int oldSrcIdx, int newSrcIdx, int destIdx, int amount, int idSource) {
        if (idSource >= 0)
        {
            midiEngine.changeModulationSource(destIdx, idSource, newSrcIdx);
            voiceArch->removeModulationEntry(oldSrcIdx, destIdx);
            voiceArch->addModulationEntry(newSrcIdx, destIdx, amount, idSource);
        }
        else
        {
            // No slot was ever established on hardware (e.g. destination was already at
            // its 6-source cap) -- nothing to change in place, treat as a fresh add.
            int nextIdSrc = voiceArch->getNextIdSourceForDest(destIdx);
            midiEngine.addModulation(newSrcIdx, destIdx, amount, nextIdSrc);
            voiceArch->removeModulationEntry(oldSrcIdx, destIdx);
            voiceArch->addModulationEntry(newSrcIdx, destIdx, amount, nextIdSrc);
            fullModMatrix->updateEntrySource(oldSrcIdx, destIdx, newSrcIdx);
        }
    };

    // Source change FROM modSummary: fullModMatrix still has oldSrcIdx → update in-place.
    voiceArch->setOnSourceChanged([this](int oldSrcIdx, int newSrcIdx, int destIdx, int amount, int idSource) {
        if (idSource >= 0)
        {
            midiEngine.changeModulationSource(destIdx, idSource, newSrcIdx);
            fullModMatrix->updateEntrySource(oldSrcIdx, destIdx, newSrcIdx);
        }
        else
        {
            int nextIdSrc = voiceArch->getNextIdSourceForDest(destIdx);
            midiEngine.addModulation(newSrcIdx, destIdx, amount, nextIdSrc);
            fullModMatrix->updateEntrySource(oldSrcIdx, destIdx, newSrcIdx);
        }
    });

    // Empty slot SRC picker: create a new mod entry using the pending amount (default +63).
    voiceArch->setOnNewSourceSelected([this](int srcIdx, int destIdx) {
        juce::Component::SafePointer<EditorTabComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis, srcIdx, destIdx] {
            if (safeThis == nullptr) return;
            int nextIdSrc = safeThis->voiceArch->getNextIdSourceForDest(destIdx);
            int initAmt   = safeThis->modAssignmentLogic.getPendingAmount();
            safeThis->midiEngine.addModulation(srcIdx, destIdx, initAmt, nextIdSrc);
            safeThis->voiceArch->addModulationEntry(srcIdx, destIdx, initAmt, nextIdSrc);
            safeThis->fullModMatrix->addEntry(srcIdx, destIdx, initAmt, nextIdSrc);
            // Light the destination LED immediately without waiting for patch dump.
            int paramID = ModAssignmentLogic::getParamIDForDestination(destIdx);
            if (paramID >= 0)
                safeThis->modAssignmentLogic.addOptimisticRoute(paramID);
            if (auto* top = safeThis->getTopLevelComponent()) top->repaint();
        });
    });
}

EditorTabComponent::~EditorTabComponent()
{
    saveSettings();
    midiEngine.removeListener(this);
    // Detach the custom LookAndFeel before it (and the buttons) are destroyed.
    for (auto* b : { &btnMod, &btnAdv, &btnRndMorph,
                     &btnInit_, &btnMute_, &btnTuneAll_ })
        b->setLookAndFeel(nullptr);
    btnCc.setLookAndFeel(nullptr);
    btnMidi.setLookAndFeel(nullptr);
}

void EditorTabComponent::setCurrentProgramNumber(int progNum)
{
    progNum = juce::jlimit(0, 99, progNum);
    titleBar_->setProgramNumber(progNum);
    // dontSendNotification: this mirrors state into the selector, it doesn't originate a change.
    programSelector.setSelectedId(progNum + 1, juce::dontSendNotification);
}

void EditorTabComponent::loadPatchFromHardware(int progNum)
{
    progNum = juce::jlimit(0, 99, progNum);
    setCurrentProgramNumber(progNum);
    // Mirror the Get-Patch flow: select the program, then request its dump 100ms later.
    midiEngine.sendProgramChange(progNum);
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::Timer::callAfterDelay(100, [safeThis, progNum] {
        if (safeThis != nullptr)
            safeThis->midiEngine.requestPatchDump(progNum);
    });
}

void EditorTabComponent::confirmThenRun(const juce::String& title,
                                        const juce::String& message,
                                        const juce::String& okText,
                                        std::function<void()> onConfirm)
{
    // Shared Load/Cancel confirmation for the nav-bar action buttons. Matches
    // PatchBrowserPanel's remove-flow pattern: heap AlertWindow + SafePointer +
    // ModalCallbackFunction, alert deleted on every path.
    auto* alert = new juce::AlertWindow(title, message,
                                        juce::MessageBoxIconType::QuestionIcon);
    alert->addButton(okText,   1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<EditorTabComponent> safe(this);
    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safe, alert, onConfirm](int result) {
            if (safe && result == 1 && onConfirm)
                onConfirm();
            delete alert;
        }), true);
}

void EditorTabComponent::initPatchWithConfirm()
{
    // Init replaces the current editor + synth patch — confirm first (the button is
    // always visible on the nav bar).
    confirmThenRun("Init Patch",
                   "Load the init patch? This replaces the current patch in the "
                   "editor and sends it to the synth.",
                   "Load",
                   [this] { if (midiEngine.loadInitPatch()) midiEngine.sendPatchToSynth(); });
}

void EditorTabComponent::tuneAllWithConfirm()
{
    // Auto-tune runs a full VCO/VCF calibration and takes a few minutes — confirm
    // first so an accidental click doesn't lock the synth into a long tune.
    confirmThenRun("Tune All",
                   "Run auto-tune now? This calibrates all VCOs and VCFs and takes "
                   "a few minutes to complete.",
                   "Tune",
                   [this] { midiEngine.sendTuneAll(); });
}

void EditorTabComponent::openOrClosePatchBrowser()
{
    if (!patchBrowserPanel_)
    {
        patchBrowserPanel_ = std::make_unique<PatchBrowserPanel>(patchLibrary_, midiEngine);
        addChildComponent(*patchBrowserPanel_);   // hidden until toggled — so the first
                                                  // click's show=!isVisible() opens it (1 click)

        patchBrowserPanel_->onClose = [this] {
            patchBrowserPanel_->setVisible(false);
        };

        patchBrowserPanel_->onPatchLoaded = [this](const PatchEntry&, int) {
            // VFD label is updated automatically via onPatchReceived when the new patch broadcasts
        };

        patchBrowserPanel_->onPatchRenamed = [this](const juce::String& newName) {
            currentPatchName = newName.substring(0, 8).toUpperCase();
            titleBar_->setPatchName(currentPatchName);
        };

        patchBrowserPanel_->getCurrentSysex = [this]() -> std::vector<uint8_t> {
            auto sysex = buildCurrentPatchSysex();
            midiEngine.setCachedPatch(sysex);
            return sysex;
        };

        patchBrowserPanel_->getCurrentPatchName = [this]() { return currentPatchName; };
    }

    bool show = !patchBrowserPanel_->isVisible();
    if (show)
    {
        const int pw = 540, ph = 540;
        auto nameR = titleBar_->getVfdNameBounds();
        int x = juce::jlimit(0, juce::jmax(0, getWidth() - pw),
                             nameR.getCentreX() - pw / 2);
        int y = nameR.getBottom() + 4;
        // Ensure the panel stays within the component bounds vertically
        if (y + ph > getHeight()) y = juce::jmax(54, getHeight() - ph);
        patchBrowserPanel_->setBounds(x, y, pw, ph);
        patchBrowserPanel_->toFront(false);
        patchBrowserPanel_->refresh();
    }
    patchBrowserPanel_->setVisible(show);
}

void EditorTabComponent::openMorphBBrowser()
{
    if (!morphBBrowserPanel_)
    {
        morphBBrowserPanel_ = std::make_unique<PatchBrowserPanel>(patchLibrary_, midiEngine);
        morphBBrowserPanel_->setSelectOnlyMode(true);   // selecting B must NOT load into editor (would clobber Patch A)
        addChildComponent(*morphBBrowserPanel_);   // hidden until toggled — first click opens (1 click)

        morphBBrowserPanel_->onClose = [this] {
            morphBBrowserPanel_->setVisible(false);
        };

        // On selection: read the file bytes directly (bypasses engine load/send),
        // hand to the morph panel as Patch B, then close the picker.
        morphBBrowserPanel_->onPatchLoaded = [this](const PatchEntry& entry, int)
        {
            juce::MemoryBlock data;
            if (!entry.file.loadFileAsData(data)) return;
            std::vector<uint8_t> sysex(static_cast<const uint8_t*>(data.getData()),
                                       static_cast<const uint8_t*>(data.getData()) + data.getSize());
            if (morphPanel_)
                morphPanel_->setPatchBFromSysex(sysex);
            morphBBrowserPanel_->setVisible(false);
        };
    }

    if (!morphBBrowserPanel_->isVisible())
    {
        const int pw = 540, ph = 540;
        int x = juce::jlimit(0, juce::jmax(0, getWidth() - pw), getWidth() / 2 - pw / 2);
        int y = juce::jlimit(0, juce::jmax(0, getHeight() - ph), 60);
        morphBBrowserPanel_->setBounds(x, y, pw, ph);
        morphBBrowserPanel_->refresh();
        morphBBrowserPanel_->setVisible(true);
        morphBBrowserPanel_->toFront(false);
    }
    else
    {
        morphBBrowserPanel_->setVisible(false);
    }
}

void EditorTabComponent::onMidiActivity(bool isInput)
{
    titleBar_->flashMidiLed(isInput);
}

void EditorTabComponent::onStatusUpdate(const juce::String& msg)
{
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, msg] {
        if (safeThis == nullptr) return;
        // Maintain ring buffer regardless of visibility.
        safeThis->lastLogLines.add(msg);
        while (safeThis->lastLogLines.size() > kLogCap)
            safeThis->lastLogLines.remove(0);
        // Only push to the TextEditor when it is actually shown — avoids
        // rebuilding the entire document on every incoming MIDI message.
        if (safeThis->logEditor.isShowing()) {
            safeThis->logEditor.moveCaretToEnd();
            safeThis->logEditor.insertTextAtCaret(msg + "\n");
        }
    });
}

void EditorTabComponent::onParameterChangedFromHardware(int page, int paramCol, int value, bool isDelta, bool isButton)
{
    // Reverse-lookup via pre-built page map — O(~10) per page instead of O(350).
    auto pageIt = paramsByPage_.find(page);
    if (pageIt == paramsByPage_.end()) return;

    for (const auto* defPtr : pageIt->second)
    {
        const auto& def = *defPtr;

        if (def.isBitmask)
        {
            int numBits = def.choices.size();
            if (paramCol >= def.paramCol && paramCol < def.paramCol + numBits)
            {
                // Bitmask params are always absolute (button subpage), never delta
                int bitIndex = paramCol - def.paramCol;
                juce::Component::SafePointer<EditorTabComponent> safeThis(this);
                juce::MessageManager::callAsync([safeThis, id = def.id, bitIndex, value] {
                    if (safeThis == nullptr) return;
                    safeThis->midiEngine.setHardwareUpdateMode(true);
                    int mask = safeThis->voiceArch->getParameterValue(id);
                    if (value) mask |=  (1 << bitIndex);
                    else       mask &= ~(1 << bitIndex);
                    safeThis->voiceArch->updateParameter(id, mask);
                    safeThis->midiEngine.setHardwareUpdateMode(false);
                });
                return;
            }
        }
        else if (def.paramCol == paramCol && (def.isButton == isButton || (def.isRadioLED && isButton)))
        {
            juce::Component::SafePointer<EditorTabComponent> safeThis(this);
            if (isDelta) {
                int minVal = def.min, maxVal = def.max;
                juce::MessageManager::callAsync([safeThis, id = def.id, delta = value, minVal, maxVal] {
                    if (safeThis == nullptr) return;
                    safeThis->midiEngine.setHardwareUpdateMode(true);
                    int current = safeThis->voiceArch->getParameterValue(id);
                    int newVal  = juce::jlimit(minVal, maxVal, current + delta);
                    safeThis->voiceArch->updateParameter(id, newVal);
                    safeThis->advancedPanel->updateParameter(id, newVal);
                    safeThis->midiEngine.setHardwareUpdateMode(false);
                });
            } else {
                juce::MessageManager::callAsync([safeThis, id = def.id, value] {
                    if (safeThis == nullptr) return;
                    safeThis->midiEngine.setHardwareUpdateMode(true);
                    safeThis->voiceArch->updateParameter(id, value);
                    safeThis->advancedPanel->updateParameter(id, value);
                    safeThis->midiEngine.setHardwareUpdateMode(false);
                });
            }
            return;
        }
    }
}

void EditorTabComponent::onSynthInputDetected(const juce::String& /*portName*/)
{
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (safeThis == nullptr) return;
        safeThis->saveSettings();
        if (safeThis->midiSettingsPanel_)
            safeThis->midiSettingsPanel_->refresh();   // light the SYNTH presence LED
    });
}

void EditorTabComponent::onSysexIDDetected(int id)
{
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, id] {
        if (safeThis == nullptr) return;
        safeThis->idSelector.setSelectedId(id + 1, juce::dontSendNotification);
        safeThis->saveSettings();
        if (safeThis->midiSettingsPanel_)
            safeThis->midiSettingsPanel_->refresh();
    });
}

void EditorTabComponent::onMidiOutputChanged(const juce::String& name)
{
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, name] {
        if (safeThis == nullptr) return;
        safeThis->midiOutputSelector.setText(name, juce::dontSendNotification);
    });
}

void EditorTabComponent::onProgramChangeReceived(int programNumber)
{
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, programNumber] {
        if (safeThis == nullptr) return;
        if (programNumber >= 0 && programNumber < 100)
            safeThis->setCurrentProgramNumber(programNumber);
        // Request a patch dump after a short delay to let the synth finish loading
        juce::Timer::callAfterDelay(100, [safeThis, programNumber] {
            if (safeThis != nullptr)
                safeThis->midiEngine.requestPatchDump(programNumber);
        });
    });
}


void EditorTabComponent::onPatchSentToSynth(int programNumber)
{
    // sendPatchToSynth() always redirects to the scratchpad slot, which can differ
    // from whatever program number the patch's SysEx header carried (e.g. a bank/file
    // origin). Correct the displayed number to the slot the patch actually landed on.
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, programNumber] {
        if (safeThis == nullptr) return;
        safeThis->setCurrentProgramNumber(programNumber);
    });
}

void EditorTabComponent::onModulationRoutingChangedByHardware()
{
    // Called on the MIDI thread. Increment generation first; the callAsync lambda captures the
    // new value. Only the last pending callAfterDelay fires the dump — rapid hardware knob
    // turns (many messages/sec) coalesce into one dump after 150ms of quiet.
    int gen = ++modDumpGeneration_;
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, gen] {
        if (safeThis == nullptr) return;
        juce::Timer::callAfterDelay(150, [safeThis, gen] {
            if (safeThis != nullptr && gen == safeThis->modDumpGeneration_.load())
                safeThis->midiEngine.requestPatchDump(safeThis->midiEngine.getCurrentProgram());
        });
    });
}

// PatchCodec::decode / PatchCodec::encode live in PatchCodec.h (namespace PatchCodec).

std::map<int,int> EditorTabComponent::currentParamMap() const
{
    // Seed params from the last received patch — this preserves PAGE2-only parameters
    // (ENV mode/trig flags, VCO/VCF mod flags, RAMP trig, LFO adv) that have no live
    // UI representation in voiceArch. Live UI values overwrite below.
    std::map<int,int> params;
    if (!lastRawPatch.empty())
        params = PatchCodec::decode(lastRawPatch);

    // Override with current live UI values: main editor first, PAGE2 panel as fallback.
    for (const auto& def : XpanderParams)
    {
        int val = voiceArch->getParameterValue(def.id);
        if (val == -999) val = advancedPanel->getParameterValue(def.id);
        if (val != -999) params[def.id] = val;
    }
    return params;
}

std::array<uint8_t,60> EditorTabComponent::currentModBytes() const
{
    std::array<uint8_t,60> mod{};
    fullModMatrix->getCurrentModBytes(mod);
    return mod;
}

std::vector<uint8_t> EditorTabComponent::encodePatchSysex(const std::map<int,int>& params,
                                                          const uint8_t* modBytes,
                                                          const juce::String& nameIn) const
{
    std::vector<uint8_t> raw = PatchCodec::encode(params);  // 196 bytes (mod region = 0)

    // Mod matrix (bytes 128–187): PatchCodec::encode leaves these zero, so write the
    // caller's 60-byte block (preserved-from-load for saves, randomized for rolls).
    if (modBytes != nullptr)
        for (int i = 0; i < 60; ++i)
            raw[(size_t)(128 + i)] = modBytes[i];

    // Patch name into bytes 188–195 (8 ASCII chars, space-padded)
    {
        juce::String name = nameIn;
        name = (name == "--------") ? "        " : name.substring(0, 8).paddedRight(' ', 8);
        for (int i = 0; i < 8; ++i)
            raw[(size_t)(188 + i)] = (uint8_t)(name[i] & 0x7F);
    }

    // Pack each byte into two 7-bit MIDI bytes, wrap in SysEx header.
    // Format: F0 10 [sysexID] 01 00 00 [392 packed bytes] F7 = 399 bytes total
    std::vector<uint8_t> sysex;
    sysex.reserve(399);
    sysex.push_back(0xF0);
    sysex.push_back(0x10);
    sysex.push_back((uint8_t)midiEngine.getSysexID());
    sysex.push_back(0x01);  // single patch dump command
    sysex.push_back(0x00);
    sysex.push_back(0x00);  // program 0 = edit buffer
    for (uint8_t b : raw)
    {
        sysex.push_back(b & 0x7F);          // lo: bits 0-6
        sysex.push_back((b >> 7) & 0x01);   // hi: bit 7
    }
    sysex.push_back(0xF7);

    jassert(sysex.size() == 399);
    return sysex;
}

std::vector<uint8_t> EditorTabComponent::buildCurrentPatchSysex() const
{
    auto mod = currentModBytes();
    return encodePatchSysex(currentParamMap(), mod.data(), currentPatchName);
}

// --- Randomizer (TASK-0) ----------------------------------------------------
// A roll = produce a randomized 399-byte patch, then run it through the normal
// load path (UI update) + send path (hardware scratchpad 99), exactly like a
// loaded patch. A one-level snapshot taken before each roll powers Revert.
void EditorTabComponent::applyRandomizedPatch(const std::map<int,int>& params,
                                              const std::array<uint8_t,60>& modBytes)
{
    auto sysex = encodePatchSysex(params, modBytes.data(), currentPatchName);
    midiEngine.setCachedPatch(sysex);   // 399-byte cache
    midiEngine.broadcastCachedPatch();  // drives UI via onPatchReceived (params + mod summary + name)
    // TASK-07: swapping patches on the synth while a note is held/sustaining can leave
    // that voice's gate orphaned on this hardware (reported as "stuck notes" after
    // repeated randomize rolls). Silence everything right before the new patch lands.
    midiEngine.sendAllNotesOff();
    midiEngine.sendPatchToSynth();      // → hardware scratchpad slot 99 (BUG-32)
}

void EditorTabComponent::doNudge()
{
    auto cfg = randomizerPanel_->getConfig();
    revertSysex_ = buildCurrentPatchSysex();            // snapshot for one-level undo
    juce::Random rng((juce::int64) juce::Time::getHighResolutionTicks());
    auto params = PatchRandomizer::nudge(currentParamMap(), cfg, rng);
    applyRandomizedPatch(params, currentModBytes());    // nudge leaves the mod matrix as-is
    randomizerPanel_->setRevertEnabled(true);
}

void EditorTabComponent::doRandomize()
{
    auto cfg = randomizerPanel_->getConfig();
    revertSysex_ = buildCurrentPatchSysex();
    juce::Random rng((juce::int64) juce::Time::getHighResolutionTicks());
    auto params = PatchRandomizer::randomize(currentParamMap(), cfg, rng);
    auto mod    = cfg.isOn(PatchRandomizer::MOD)
                    ? PatchRandomizer::randomizeModBytes(currentModBytes(), cfg, rng)
                    : currentModBytes();
    applyRandomizedPatch(params, mod);
    randomizerPanel_->setRevertEnabled(true);
}

void EditorTabComponent::doRevert()
{
    if (revertSysex_.size() != 399) return;
    midiEngine.setCachedPatch(revertSysex_);
    midiEngine.broadcastCachedPatch();
    midiEngine.sendPatchToSynth();
    revertSysex_.clear();
    randomizerPanel_->setRevertEnabled(false);
}

void EditorTabComponent::onPatchReceived(const std::vector<uint8_t>& rawPatchBytes, int progNum)
{
    juce::Component::SafePointer<EditorTabComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, rawPatchBytes, progNum] {
        if (safeThis == nullptr)
        {
            // safePointer is null — EditorTabComponent was destroyed before callback ran
            return;
        }

        safeThis->midiEngine.setHardwareUpdateMode(true);
        safeThis->lastRawPatch = rawPatchBytes;
        auto params = PatchCodec::decode(rawPatchBytes);

        for (auto& [id, value] : params) {
            safeThis->voiceArch->updateParameter(id, value);
            safeThis->advancedPanel->updateParameter(id, value);
        }

        std::vector<int> intBytes(rawPatchBytes.begin(), rawPatchBytes.end());
        safeThis->voiceArch->updateModSummary(intBytes);
        safeThis->fullModMatrix->updateFromPatch(intBytes);

        // Decode patch name: 8 ASCII bytes at raw offsets 188–195
        if (rawPatchBytes.size() >= 196)
        {
            juce::String name;
            for (int i = 188; i < 196; ++i)
                name += (char)(rawPatchBytes[i] & 0x7F);
            name = name.trimEnd().toUpperCase();
            if (name.isEmpty()) name = "--------";
            safeThis->currentPatchName = name;
            safeThis->titleBar_->setPatchName(name);

            // Refresh Morph "A" from any real patch load (so loading a new patch while
            // the RND/MORPH pane is open updates A). Skip morph results, which are named
            // "M-..." — otherwise A would chase its own output and the "M-" prefix would
            // double up.
            if (safeThis->morphPanel_ && !name.startsWith("M-"))
                safeThis->morphPanel_->setPatchARaw(rawPatchBytes);
        }

        safeThis->midiEngine.setHardwareUpdateMode(false);

        // Keep the displayed patch number in sync with the loaded patch.
        safeThis->setCurrentProgramNumber(progNum);

        // Force a full repaint of the editor area so all updated controls redraw.
        safeThis->voiceArch->repaint();
    });
}

void EditorTabComponent::loadSettings()
{
    if (!appProperties) return;

    int savedProg = appProperties->getIntValue("ProgramNumber", 1);
    programSelector.setSelectedId(savedProg, juce::dontSendNotification);
    midiEngine.setCurrentProgram(savedProg - 1);  // seed nav SysEx delta base
    titleBar_->setProgramNumber(savedProg - 1);

    // In standalone mode the AudioDeviceManager restores its own MIDI input and output
    // state; MidiEngine picks them up via the deviceManager bridge automatically.
    // In AU mode we manage MIDI I/O ourselves and restore from our settings file.
    if (!midiEngine.isUsingExternalDeviceManager())
    {
        // Restore MIDI inputs (multi-input key, falling back to legacy single key)
        juce::String savedInputs = appProperties->getValue("MidiInputs");
        if (savedInputs.isEmpty())
            savedInputs = appProperties->getValue("MidiInput");
        if (savedInputs.isNotEmpty())
        {
            auto available = midiEngine.getMidiInputs();
            for (const auto& tok : juce::StringArray::fromTokens(savedInputs, ",", ""))
            {
                auto name = tok.trim();
                if (name.isNotEmpty() && available.contains(name))
                    midiEngine.addMidiInput(name);
            }
        }

        // Restore MIDI output
        juce::String savedOut = appProperties->getValue("MidiOutput");
        if (savedOut.isNotEmpty() && midiEngine.getMidiOutputs().contains(savedOut))
        {
            midiEngine.setMidiOutput(savedOut);
            midiOutputSelector.setText(savedOut, juce::dontSendNotification);
        }
    }

    // Restore sysex ID (persisted for all builds). Only apply if a value was actually
    // saved — on a fresh install (no key) leave the in-code default in place so it
    // isn't overwritten with 0, which is unlikely to match the hardware's real ID.
    if (appProperties->containsKey("SysexID"))
    {
        int savedId = appProperties->getIntValue("SysexID");
        if (savedId >= 0 && savedId < 16)
        {
            midiEngine.setSysexID(savedId);
            idSelector.setSelectedId(savedId + 1, juce::dontSendNotification);
        }
    }

    // Restore manual synth-type flag (cannot be auto-detected — see MidiEngine.h).
    if (appProperties->containsKey("SynthTypeIsMatrix12"))
        midiEngine.setSynthTypeIsMatrix12(appProperties->getBoolValue("SynthTypeIsMatrix12"));

    // Restore auto-detected synth input (only if the port is still available).
    // If not available, synthInputName stays empty and will re-detect from the next
    // incoming Oberheim SysEx message.
    {
        juce::String savedSynthIn = appProperties->getValue("SynthInput");
        if (savedSynthIn.isNotEmpty() && midiEngine.getMidiInputs().contains(savedSynthIn))
            midiEngine.setSynthInput(savedSynthIn);
    }

    auto outName = midiEngine.getCurrentMidiOutputName();
    midiOutputSelector.setText(outName, juce::dontSendNotification);

    if (ccMapPanel_)
    {
        midiEngine.loadCcMap(*appProperties);
        ccMapPanel_->refresh();
    }
}


void EditorTabComponent::saveSettings()
{
    if (!appProperties) return;
    // In standalone mode the AudioDeviceManager persists its own MIDI state.
    // In AU mode we own the device manager and must save/restore it ourselves.
    if (!midiEngine.isUsingExternalDeviceManager())
    {
        auto enabled = midiEngine.getEnabledMidiInputNames();
        appProperties->setValue("MidiInputs", enabled.joinIntoString(","));
        appProperties->setValue("MidiInput", enabled.isEmpty() ? juce::String() : enabled[0]);
        appProperties->setValue("MidiOutput", midiEngine.getCurrentMidiOutputName());
    }
    appProperties->setValue("SysexID", midiEngine.getSysexID());
    appProperties->setValue("SynthTypeIsMatrix12", midiEngine.isSynthTypeMatrix12());
    appProperties->setValue("SynthInput", midiEngine.getSynthInputName());
    appProperties->setValue("ProgramNumber", programSelector.getSelectedId());
    if (ccMapPanel_)
        midiEngine.saveCcMap(*appProperties);
    appProperties->save();
}

void EditorTabComponent::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();
    g.fillAll(theme.mainBackground);

    // ---- Nav bar ----
    auto navR = juce::Rectangle<int>(0, getHeight() - 32, getWidth(), 32);
    g.setColour(juce::Colour(0xff1c1c1c));
    g.fillRect(navR);
    g.setColour(juce::Colour(0xff121212));
    g.fillRect(navR.getX(), navR.getY(), navR.getWidth(), 2);

    // Version string in nav bar (left).
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    g.setColour(theme.textHeader.withAlpha(0.25f));
    g.drawText("v" JucePlugin_VersionString, navR.withTrimmedLeft(8).withWidth(60),
               juce::Justification::centredLeft);
}

void EditorTabComponent::resized()
{
    auto area = getLocalBounds();

    titleBar_->setBounds(area.removeFromTop(54));

    // ---- Nav bar (32px at bottom) ----
    auto navBar = area.removeFromBottom(32);
    bottomPaneManager_.layoutNavButtons(navBar, 90, 24, 4);

    // ---- Bottom pane (conditional, fits below row 3 within existing window) ----
    if (bottomPaneManager_.isOpen()) {
        auto paneArea = area.removeFromBottom(BottomPaneManager::kPaneHeight);
        // voiceArch outer edge = x=10 (area.reduce(10,0) below); internally it does
        // reduced(10), putting column content at x=20. Pane gets the same outer edge
        // (reduce(10,0)) so its internal reduced(10) also lands at x=20 — columns align.
        paneArea.reduce(10, 0);
        bottomPaneManager_.layoutPane(paneArea);
    } else {
        bottomPaneManager_.hideAll();
    }

    // ---- Main content: voiceArch fills remaining area. ----
    // When pane is open, omit the bottom reduce so voiceArch sits flush above the pane.
    if (bottomPaneManager_.isOpen()) {
        area.reduce(10, 0);
        area.removeFromTop(10);
    } else {
        area.reduce(10, 10);
    }
    voiceArch->setBounds(area);
}

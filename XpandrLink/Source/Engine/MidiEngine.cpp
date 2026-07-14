#include "MidiEngine.h"
#include "../Data/OberheimDefs.h"
#include "../Data/XpanderData.h"
#include "BinaryData.h"

MidiEngine::MidiEngine()
{
    deviceManager.addChangeListener(this);
    startTimer(5);  // drain send queue at ~5 ms resolution
}

MidiEngine::~MidiEngine()
{
    stopTimer();
    sendQueue.clear();
    masterReference.clear();
    auto& dm = activeManager();
    dm.removeChangeListener(this);
    for (auto& d : juce::MidiInput::getAvailableDevices()) {
        if (dm.isMidiInputDeviceEnabled(d.identifier)) {
            dm.removeMidiInputDeviceCallback(d.identifier, this);
            dm.setMidiInputDeviceEnabled(d.identifier, false);
        }
    }
    const juce::ScopedLock sl(listenerLock);
    listeners.clear();
}

juce::AudioDeviceManager& MidiEngine::activeManager()
{
    return externalDeviceManager ? *externalDeviceManager : deviceManager;
}

juce::AudioDeviceManager& MidiEngine::getDeviceManager()
{
    return activeManager();
}

void MidiEngine::useDeviceManager(juce::AudioDeviceManager& dm)
{
    // Detach from internal manager: remove listener and disable any enabled inputs
    auto& current = activeManager();
    current.removeChangeListener(this);
    for (auto& d : juce::MidiInput::getAvailableDevices()) {
        if (current.isMidiInputDeviceEnabled(d.identifier)) {
            current.removeMidiInputDeviceCallback(d.identifier, this);
            current.setMidiInputDeviceEnabled(d.identifier, false);
        }
    }

    externalDeviceManager = &dm;
    dm.addChangeListener(this);
    syncMidiInputCallbacks();
    syncMidiOutputFromDeviceManager();
}

void MidiEngine::changeListenerCallback(juce::ChangeBroadcaster*)
{
    syncMidiInputCallbacks();
    syncMidiOutputFromDeviceManager();
}

void MidiEngine::syncMidiInputCallbacks()
{
    auto& dm = activeManager();
    for (auto& d : juce::MidiInput::getAvailableDevices()) {
        if (dm.isMidiInputDeviceEnabled(d.identifier))
            dm.addMidiInputDeviceCallback(d.identifier, this);
    }
}

void MidiEngine::syncMidiOutputFromDeviceManager()
{
    if (externalDeviceManager == nullptr) return;

    auto id = externalDeviceManager->getDefaultMidiOutputIdentifier();
    juce::String deviceName;
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.identifier == id) { deviceName = d.name; break; }

    if (deviceName != getCurrentMidiOutputName())
        setMidiOutput(deviceName);
}

void MidiEngine::setMidiInput(const juce::String& deviceName)
{
    // Disable all currently-enabled inputs, then enable just the named one.
    auto& dm = activeManager();
    for (auto& d : juce::MidiInput::getAvailableDevices()) {
        if (dm.isMidiInputDeviceEnabled(d.identifier)) {
            dm.removeMidiInputDeviceCallback(d.identifier, this);
            dm.setMidiInputDeviceEnabled(d.identifier, false);
        }
    }
    if (deviceName.isNotEmpty())
        addMidiInput(deviceName);
}

void MidiEngine::addMidiInput(const juce::String& deviceName)
{
    auto& dm = activeManager();
    for (auto& d : juce::MidiInput::getAvailableDevices()) {
        if (d.name == deviceName) {
            dm.setMidiInputDeviceEnabled(d.identifier, true);
            dm.addMidiInputDeviceCallback(d.identifier, this);
            juce::String msg = "Connected Input: " + d.name;
            const juce::ScopedLock sl(listenerLock);
            DBG(msg);
            for (auto* l : listeners) l->onStatusUpdate(msg);
            return;
        }
    }
}

void MidiEngine::removeMidiInput(const juce::String& deviceName)
{
    auto& dm = activeManager();
    for (auto& d : juce::MidiInput::getAvailableDevices()) {
        if (d.name == deviceName) {
            dm.removeMidiInputDeviceCallback(d.identifier, this);
            dm.setMidiInputDeviceEnabled(d.identifier, false);
            juce::String msg = "Disconnected Input: " + d.name;
            const juce::ScopedLock sl(listenerLock);
            DBG(msg);
            for (auto* l : listeners) l->onStatusUpdate(msg);
            return;
        }
    }
}

juce::StringArray MidiEngine::getEnabledMidiInputNames() const
{
    auto& dm = const_cast<MidiEngine*>(this)->activeManager();
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices()) {
        if (dm.isMidiInputDeviceEnabled(d.identifier))
            names.add(d.name);
    }
    return names;
}

void MidiEngine::setMidiOutput(const juce::String& deviceName) {
    auto list = juce::MidiOutput::getAvailableDevices();
    juce::String openedIdentifier;
    {
        // Lock: the MIDI thread dereferences activeOutput in the thru-forward path.
        const juce::ScopedLock sl(listenerLock);
        activeOutput.reset();
        for (auto& device : list) {
            if (device.name == deviceName) {
                activeOutput = juce::MidiOutput::openDevice(device.identifier);
                openedIdentifier = device.identifier;
                break;
            }
        }
    }
    // Keep the shared device manager's default output in agreement, otherwise
    // its next change broadcast makes syncMidiOutputFromDeviceManager() revert
    // (or disconnect) this selection. No-op guard prevents broadcast loops.
    if (externalDeviceManager != nullptr
        && externalDeviceManager->getDefaultMidiOutputIdentifier() != openedIdentifier)
        externalDeviceManager->setDefaultMidiOutputDevice(openedIdentifier);
    sendQueue.clear();
    lastSentPage   = -1;
    lastSentMode   = -1;
    lastQueuedPage = -1;
    lastQueuedMode = -1;
    juce::String name = activeOutput ? activeOutput->getName() : juce::String();
    const juce::ScopedLock sl(listenerLock);
    for (auto* l : listeners) l->onMidiOutputChanged(name);
}

void MidiEngine::setSysexID(int id) { sysexID = id; }

void MidiEngine::setSynthInput(const juce::String& deviceName)
{
    const juce::ScopedLock sl(listenerLock);
    synthInputName = deviceName;
}

juce::String MidiEngine::getSynthInputName() const
{
    const juce::ScopedLock sl(listenerLock);
    return synthInputName;
}

bool MidiEngine::hasSeenSynthThisSession() const
{
    const juce::ScopedLock sl(listenerLock);
    return synthSeenThisSession;
}

void MidiEngine::broadcastParameterChange(int page, int paramCol, int value, bool isDelta, bool isButton) {
    const juce::ScopedLock sl(listenerLock);
    for (auto* l : listeners)
        l->onParameterChangedFromHardware(page, paramCol, value, isDelta, isButton);
}

juce::String MidiEngine::getCurrentMidiInputName() const
{
    auto names = getEnabledMidiInputNames();
    return names.isEmpty() ? juce::String() : names[0];
}

juce::String MidiEngine::getCurrentMidiOutputName() const {
    if (activeOutput != nullptr)
        return activeOutput->getName();
    return {};
}

juce::StringArray MidiEngine::getMidiInputs() const {
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices()) names.add(d.name);
    return names;
}

juce::StringArray MidiEngine::getMidiOutputs() const {
    juce::StringArray names;
    for (auto& d : juce::MidiOutput::getAvailableDevices()) names.add(d.name);
    return names;
}

void MidiEngine::sendSysex(const std::vector<unsigned char>& data) {
    if (activeOutput && !data.empty()) {
        // createSysExMessage adds its own F0/F7 wrapper.
        // Strip them from the data if the caller already included them.
        const uint8_t* sysexData = data.data();
        int sysexSize = (int)data.size();
        if (sysexSize >= 2 && sysexData[0] == 0xF0 && sysexData[sysexSize - 1] == 0xF7) {
            ++sysexData;
            sysexSize -= 2;
        }
        activeOutput->sendMessageNow(juce::MidiMessage::createSysExMessage(sysexData, sysexSize));
        const juce::ScopedLock sl(listenerLock);
        for (auto* l : listeners) l->onMidiActivity(false);
    }
}

// ---------------------------------------------------------------------------
// Send-queue implementation
// ---------------------------------------------------------------------------

void MidiEngine::timerCallback()
{
    auto now = juce::Time::getMillisecondCounter();

    // Flush a coalesced mod-matrix amount drag once its throttle window has passed --
    // sends only the latest value a fast drag settled on, never an intermediate tick.
    if (pendingModAmount_.valid && !shouldCoalesceModAmount(now, lastModAmountSendTime_, Oberheim::kModCmdGapMs))
    {
        auto pending = pendingModAmount_;
        pendingModAmount_.valid = false;
        sendModAmountNow(pending.destIndex, pending.idSource, pending.amount);
    }

    while (!sendQueue.empty())
    {
        // Use signed subtraction to handle uint32 wraparound (~49-day period).
        if ((juce::int32)(now - nextSendTime) < 0) break;

        auto msg = std::move(sendQueue.front());
        sendQueue.pop_front();

        if (!msg.bytes.empty())
        {
            sendSysex(msg.bytes);
            // Page-select format: F0 10 id 0B page mode F7 (7 bytes, cmd at [3]).
            // Only real parameter pages (>= PAGE_VCO1) become the hardware-knob
            // fallback page; Master/MIDI/Tune command pages (< 0x20) must not clobber
            // lastSentPage: the synth sends knob-delta messages with no preceding
            // page-select unless the front panel changed pages, so we fall back to
            // this value to know which page an unattributed knob delta belongs to.
            if (msg.bytes.size() == 7 && msg.bytes[3] == 0x0B
                && msg.bytes[4] >= Matrix12::PAGE_VCO1)
            {
                lastSentPage = msg.bytes[4];
                lastSentMode = msg.bytes[5];
            }
            // Mod routing format: F0 10 id 0F ... (cmd at [3]).
            // Record send time so handleModRouting can suppress the hardware echo.
            if (msg.bytes.size() >= 4 && msg.bytes[3] == 0x0F)
                lastModSentTime_ = now;
        }
        else if (msg.action)
        {
            msg.action();
        }

        nextSendTime = now + (juce::uint32)msg.delayAfterMs;
        if (msg.delayAfterMs > 0) break;  // settle before next message
    }
}

void MidiEngine::enqueueMessage(std::vector<uint8_t> bytes, int delayAfterMs)
{
    sendQueue.push_back({ std::move(bytes), {}, delayAfterMs });
}

void MidiEngine::enqueueAction(std::function<void()> action, int delayAfterMs)
{
    sendQueue.push_back({ {}, std::move(action), delayAfterMs });
}

void MidiEngine::enqueuePageSelectIfNeeded(int page, int mode, int settleMs, bool force)
{
    if (!force && lastQueuedPage == page && lastQueuedMode == mode) return;
    std::vector<uint8_t> ps = {
        0xF0, 0x10, (uint8_t)(sysexID),
        0x0B, (uint8_t)page, (uint8_t)mode,
        0xF7
    };
    enqueueMessage(std::move(ps), settleMs);
    lastQueuedPage = page;
    lastQueuedMode = mode;
}

void MidiEngine::sendPageSelect(int page, int mode, bool force)
{
    // Route through the queue so display-nav page-selects don't interleave
    // with pending parameter messages. 0 ms settle — no parameter follows.
    enqueuePageSelectIfNeeded(page, mode, 0, force);
}

void MidiEngine::sendParameterToSynth(int page, int paramID, int absoluteValue, bool isButton) {
    // Block feedback: if a hardware knob/button update is being applied to the UI,
    // the slider onValueChange must not echo the value back to the synth.
    if (hardwareUpdateMode) return;

    // Notify listeners that a UI-initiated parameter change is about to be sent.
    {
        const juce::ScopedLock sl(listenerLock);
        for (auto* l : listeners)
            l->onParameterSentToSynth(page, paramID, absoluteValue, isButton);
    }

    int val_lo = absoluteValue & 0x7F;
    int val_hi = (absoluteValue >> 7) & 0x01;

    std::vector<uint8_t> paramSysex = {
        0xF0, 0x10, (uint8_t)(sysexID),
        0x0A, 0x00, (uint8_t)paramID, 0x00,
        0x00, 0x00, (uint8_t)val_lo, (uint8_t)val_hi,
        0xF7
    };

    int settleMs = isButton ? Oberheim::kButtonPageSettleMs : Oberheim::kSliderPageSettleMs;
    // Buttons always force mode=1; sliders deduplicate if the queued page already matches.
    enqueuePageSelectIfNeeded(page, isButton ? 1 : 0, settleMs, isButton);
    enqueueMessage(std::move(paramSysex), 0);
}

void MidiEngine::sendMidiMute()
{
    // Master -> MIDI -> Mute is a single-action page: selecting it triggers it.
    // force=true so a repeated click re-fires (enqueuePageSelectIfNeeded dedupes).
    sendPageSelect(Matrix12::PAGE_MIDI_MUTE, 0x00, /*force=*/true);
}

void MidiEngine::sendTuneAll()
{
    // Master -> Tune -> All (full autocalibration). The Tune-page soft-buttons are
    // NOT remotely pressable: hardware-confirmed that a received page-select navigates
    // the synth to the Tune page but the "page edit" (0A) button-press is ignored for
    // Master-section command buttons (that mechanism drives sound parameters only).
    // The one remote tune the synth honours is the MIDI Tune Request (F6), which the
    // Oberheim MIDI spec defines as "tune all" (autocal VCO freq/PW + VCF freq/res).
    // F6 is a one-byte System Common message, so send it directly rather than through
    // the SysEx queue. (There is no remote command for VCO-only tune.)
    if (activeOutput)
        activeOutput->sendMessageNow(juce::MidiMessage(0xF6));
}

void MidiEngine::sendDisplayOff()
{
    uint8_t cmd = synthTypeIsMatrix12 ? 0x06 : 0x05;
    enqueueMessage({0xF0, 0x10, (uint8_t)sysexID, cmd, 0x00, 0xF7}, Oberheim::kDisplaySettleMs);
}

void MidiEngine::sendDisplayOn()
{
    uint8_t cmd = synthTypeIsMatrix12 ? 0x06 : 0x05;
    enqueueMessage({0xF0, 0x10, (uint8_t)sysexID, cmd, 0x02, 0xF7}, Oberheim::kDisplaySettleMs);
}

void MidiEngine::sendDisplayMessage(const juce::String& text, bool typewriter)
{
    // F0 10 [id] [cmd] 01 <80 bytes, uppercased + space-padded> F7 (86 bytes total).
    // Command byte and payload length verified against XpanderController.cs
    // SendDisplayMessageToSynth / DISPLAY_INTRO_LENGTH+MAX_DISPLAY_MESSAGE_LENGTH+1.
    auto buildTextMessage = [this](const juce::String& body) {
        uint8_t cmd = synthTypeIsMatrix12 ? 0x06 : 0x05;
        juce::String padded = body.toUpperCase().substring(0, 80).paddedRight(' ', 80);
        std::vector<uint8_t> msg;
        msg.reserve(86);
        msg.push_back(0xF0); msg.push_back(0x10); msg.push_back((uint8_t)sysexID);
        msg.push_back(cmd);  msg.push_back(0x01);
        for (int i = 0; i < 80; ++i)
            msg.push_back((uint8_t)padded[i]);
        msg.push_back(0xF7);
        return msg;
    };

    sendDisplayOff();
    sendDisplayOn();

    if (!typewriter)
    {
        enqueueMessage(buildTextMessage(text), Oberheim::kDisplaySettleMs);
        return;
    }

    // Typewriter: send growing left-substrings of the (already uppercased) text,
    // each still padded to the full 80 chars, kDisplayScrollMs apart.
    juce::String upper = text.toUpperCase().substring(0, 80);
    for (int i = 0; i <= upper.length(); ++i)
        enqueueMessage(buildTextMessage(upper.substring(0, i)), Oberheim::kDisplayScrollMs);
}

// page + subPage for each EnumModulationDestinations (0-46).
// Derived from XpanderConstants.PageSubPageForModulationDestination in C# reference.
struct ModDest { uint8_t page, subPage; };
static const ModDest kModDestTable[] = {
    {0x20,2},{0x20,5},{0x20,7},             // VCO1: FRQ, PW, VOL
    {0x21,2},{0x21,5},{0x21,7},             // VCO2: FRQ, PW, VOL
    {0x22,2},{0x22,3},{0x22,6},{0x22,7},    // VCF: FRQ, RES; VCA1, VCA2
    {0x30,2},{0x30,7},                      // LFO1: SPD, AMP
    {0x31,2},{0x31,7},                      // LFO2: SPD, AMP
    {0x32,2},{0x32,7},                      // LFO3: SPD, AMP
    {0x33,2},{0x33,7},                      // LFO4: SPD, AMP
    {0x34,2},{0x34,7},                      // LFO5: SPD, AMP
    {0x28,2},{0x28,3},{0x28,4},{0x28,6},{0x28,7}, // ENV1: DLY,ATK,DCY,REL,AMP
    {0x29,2},{0x29,3},{0x29,4},{0x29,6},{0x29,7}, // ENV2
    {0x2A,2},{0x2A,3},{0x2A,4},{0x2A,6},{0x2A,7}, // ENV3
    {0x2B,2},{0x2B,3},{0x2B,4},{0x2B,6},{0x2B,7}, // ENV4
    {0x2C,2},{0x2C,3},{0x2C,4},{0x2C,6},{0x2C,7}, // ENV5
    {0x23,2},{0x23,6},                      // FM_AMP, LAG_RATE
};
static const int kModDestCount = (int)(sizeof(kModDestTable) / sizeof(kModDestTable[0]));

int MidiEngine::destIndexForPageSubPage(int page, int subPage)
{
    for (int i = 0; i < kModDestCount; ++i)
        if (kModDestTable[i].page == page && kModDestTable[i].subPage == subPage)
            return i;
    return -1;
}

// Hardware front-panel mod-matrix edit message (cmd=0x0F), JUCE data[] (F0/F7 excluded):
//   data[0]=0x10 data[1]=id data[2]=0x0F data[3]=0x00
//   data[4]=idSource(slot 0-5) data[5]=0x00 data[6]=command data[7]=0x00
//   data[8]=valueLo data[9]=valueHi
// Destination is not carried in the message -- resolved from the caller's current
// page/subpage context via destIndexForPageSubPage. Matches the C# reference's
// IsModulationEditFollowsSysEx (XpanderController.MIDIEvents.cs).
bool MidiEngine::decodeModulationEditMessage(const uint8_t* data, int len,
                                             int rxPage, int rxSubPage, ModEdit& out)
{
    if (len < 10) return false;

    int destIndex = destIndexForPageSubPage(rxPage, rxSubPage);
    if (destIndex < 0) return false;

    out.destIndex = destIndex;
    out.idSource   = data[4];
    out.command    = (data[6] <= (int)ModEditCommand::SetSign)
                    ? (ModEditCommand)data[6] : ModEditCommand::Unknown;
    out.value      = decodeSysexValue(data[8], data[9]);
    return true;
}

void MidiEngine::addModulation(int sourceIndex, int destIndex, int amount, int idSourceForAmtAndSign) {

    if (sourceIndex < 0 || sourceIndex > 26 || destIndex < 0 || destIndex >= kModDestCount) {
        juce::String err = "MOD ASSIGN: invalid src=" + juce::String(sourceIndex)
                         + " dst=" + juce::String(destIndex);
        const juce::ScopedLock sl(listenerLock);
        DBG(err);
        for (auto* l : listeners) l->onStatusUpdate(err);
        return;
    }

    auto dest   = kModDestTable[destIndex];
    int  absAmt = std::abs(amount);
    auto id     = (uint8_t)(sysexID);
    // Clamp idSource to valid range; fall back to 0 if unknown.
    auto idSrc  = (uint8_t)(juce::jlimit(0, 5, idSourceForAmtAndSign >= 0 ? idSourceForAmtAndSign : 0));

    // Mod matrix SysEx format (outer cmd = 0x0F, vs 0x0A for regular params):
    //   F0 10 [id] 0x0F 0x00 [IDSource] 0x00 [modCmd] 0x00 [val_lo] 0x00 F7
    // ADDSOURCE always uses IDSource=0 (synth appends new source to next slot).
    // SETSIGN / SETUNSIGNEDVALUE use the actual assigned slot (idSourceForAmtAndSign).

    // ADDSOURCE (cmd=0x00): IDSource always 0x00
    std::vector<uint8_t> addSrc = {
        0xF0, 0x10, id, 0x0F, 0x00,
        0x00, 0x00, 0x00,
        0x00, (uint8_t)sourceIndex, 0x00, 0xF7
    };
    // SETSIGN (cmd=0x07)
    std::vector<uint8_t> setSign = {
        0xF0, 0x10, id, 0x0F, 0x00,
        idSrc, 0x00, 0x07,
        0x00, (uint8_t)(amount < 0 ? 1 : 0), 0x00, 0xF7
    };
    // SETUNSIGNEDVALUE (cmd=0x03)
    std::vector<uint8_t> setAmt = {
        0xF0, 0x10, id, 0x0F, 0x00,
        idSrc, 0x00, 0x03,
        0x00, (uint8_t)absAmt, 0x00, 0xF7
    };

    enqueuePageSelectIfNeeded(dest.page, dest.subPage, Oberheim::kSliderPageSettleMs);
    enqueueMessage(std::move(addSrc), absAmt > 0 ? Oberheim::kModCmdGapMs : 0);
    if (absAmt > 0) {
        enqueueMessage(std::move(setSign), Oberheim::kModCmdGapMs);
        enqueueMessage(std::move(setAmt), 0);
    }

    juce::String msg = "MOD ASSIGN: src=" + juce::String(sourceIndex)
                     + " dst=" + juce::String(destIndex)
                     + " amt=" + juce::String(amount)
                     + " idSrc=" + juce::String((int)idSrc);
    const juce::ScopedLock sl(listenerLock);
    DBG(msg);
    for (auto* l : listeners) l->onStatusUpdate(msg);
}

void MidiEngine::removeModulation(int sourceIndex, int destIndex, int idSource) {
    if (sourceIndex < 0 || sourceIndex > 26 || destIndex < 0 || destIndex >= kModDestCount) return;

    auto dest  = kModDestTable[destIndex];
    auto id    = (uint8_t)(sysexID);
    auto idSrc = (uint8_t)(juce::jlimit(0, 5, idSource >= 0 ? idSource : 0));

    // DELETESOURCE (cmd=0x01) truly removes the hardware slot. Hardware renumbers higher
    // slots downward automatically; our decrementIdSourceAfterRemove mirrors this.
    // (Using SETUNSIGNEDVALUE=0 only zeroes the amount — the ghost slot remains, causing
    // subsequent ADDSOURCE to land at slot 1 while our IDSource tracking expects slot 0.)
    std::vector<uint8_t> delSrc = {
        0xF0, 0x10, id, 0x0F, 0x00,
        idSrc, 0x00, 0x01,   // cmd = DELETESOURCE
        0x00, 0x00, 0x00, 0xF7
    };

    enqueuePageSelectIfNeeded(dest.page, dest.subPage, Oberheim::kSliderPageSettleMs);
    enqueueMessage(std::move(delSrc), 0);

    juce::String msg = "MOD REMOVE: src=" + juce::String(sourceIndex)
                     + " dst=" + juce::String(destIndex)
                     + " idSrc=" + juce::String((int)idSrc);
    const juce::ScopedLock sl(listenerLock);
    DBG(msg);
    for (auto* l : listeners) l->onStatusUpdate(msg);
}

bool MidiEngine::shouldCoalesceModAmount(juce::uint32 now, juce::uint32 lastSendTime, int throttleMs)
{
    auto elapsed = (juce::int32)(now - lastSendTime);
    return elapsed >= 0 && elapsed <= throttleMs;
}

void MidiEngine::sendModAmountNow(int destIndex, int idSource, int newAmount)
{
    int absAmt = std::abs(newAmount);
    auto dest  = kModDestTable[destIndex];
    auto id    = (uint8_t)(sysexID);
    auto src   = (uint8_t)idSource;

    std::vector<uint8_t> setSign = {
        0xF0, 0x10, id, 0x0F, 0x00,
        src, 0x00, 0x07,   // cmd = SETSIGN
        0x00, (uint8_t)(newAmount < 0 ? 1 : 0), 0x00, 0xF7
    };
    std::vector<uint8_t> setAmt = {
        0xF0, 0x10, id, 0x0F, 0x00,
        src, 0x00, 0x03,   // cmd = SETUNSIGNEDVALUE
        0x00, (uint8_t)absAmt, 0x00, 0xF7
    };

    // No gap between SETSIGN and SETUNSIGNEDVALUE here — the slot already exists.
    // A 50ms gap here causes rapid knob-drag events to stack up settle delays (500ms+ backlog).
    // The flood risk that gap was originally meant to prevent a different way is now handled
    // by the caller's throttle/coalesce gate (changeModulationAmount), not a per-message delay.
    enqueuePageSelectIfNeeded(dest.page, dest.subPage, Oberheim::kModCmdGapMs);
    enqueueMessage(std::move(setSign), 0);
    enqueueMessage(std::move(setAmt), 0);

    lastModAmountSendTime_ = juce::Time::getMillisecondCounter();

    juce::String msg = "MOD CHANGE AMT: dest=" + juce::String(destIndex)
                     + " idSrc=" + juce::String(idSource)
                     + " amt=" + juce::String(newAmount);
    const juce::ScopedLock sl(listenerLock);
    DBG(msg);
    for (auto* l : listeners) l->onStatusUpdate(msg);
}

void MidiEngine::changeModulationAmount(int destIndex, int idSource, int newAmount) {
    if (destIndex < 0 || destIndex >= kModDestCount) return;

    // A fast slider drag can call this dozens of times a second. Sending every intermediate
    // value as its own SETSIGN+SETUNSIGNEDVALUE pair (zero delay between them, by design --
    // see sendModAmountNow) floods the synth's MIDI IN faster than its UART/firmware can keep
    // up, observed as a temporary hardware lockup that self-recovers once the flood stops
    // (found 2026-07-13, especially reproducible on VCA1/VCA2 Vol). Only the latest value
    // within a throttle window is ever actually sent -- intermediate drag ticks are coalesced.
    auto now = juce::Time::getMillisecondCounter();
    if (shouldCoalesceModAmount(now, lastModAmountSendTime_, Oberheim::kModCmdGapMs))
    {
        pendingModAmount_ = { true, destIndex, idSource, newAmount };
        return;
    }
    sendModAmountNow(destIndex, idSource, newAmount);
}

std::vector<uint8_t> MidiEngine::buildChangeSourceMessage(uint8_t sysexId, uint8_t idSource, uint8_t newSource)
{
    return {
        0xF0, 0x10, sysexId, 0x0F, 0x00,
        idSource, 0x00, 0x02,   // cmd = CHANGESOURCE
        0x00, newSource, 0x00, 0xF7
    };
}

void MidiEngine::changeModulationSource(int destIndex, int idSource, int newSourceIndex) {
    if (destIndex < 0 || destIndex >= kModDestCount
        || newSourceIndex < 0 || newSourceIndex > 26) return;

    auto dest = kModDestTable[destIndex];
    auto msg  = buildChangeSourceMessage((uint8_t)(sysexID), (uint8_t)idSource, (uint8_t)newSourceIndex);

    enqueuePageSelectIfNeeded(dest.page, dest.subPage, Oberheim::kSliderPageSettleMs);
    enqueueMessage(std::move(msg), 0);

    juce::String msg2 = "MOD CHANGE SOURCE: dest=" + juce::String(destIndex)
                      + " idSrc=" + juce::String(idSource)
                      + " newSrc=" + juce::String(newSourceIndex);
    const juce::ScopedLock sl(listenerLock);
    DBG(msg2);
    for (auto* l : listeners) l->onStatusUpdate(msg2);
}

void MidiEngine::addListener(Listener* l) {
    const juce::ScopedLock sl(listenerLock);
    jassert(l->midiEngineRef_.get() == nullptr);  // catch double-registration
    l->midiEngineRef_ = this;
    listeners.push_back(l);
}

void MidiEngine::removeListener(Listener* l) {
    const juce::ScopedLock sl(listenerLock);
    l->midiEngineRef_ = nullptr;
    listeners.erase(std::remove(listeners.begin(), listeners.end(), l), listeners.end());
}

// ---------------------------------------------------------------------------
// CC automation table
// ---------------------------------------------------------------------------

void MidiEngine::setCcMap(int cc, int paramId)
{
    if (cc < 0 || cc >= 128) return;
    CcMappedParam slot;
    slot.paramId = paramId;
    if (paramId >= 0) {
        for (const auto& p : XpanderParams) {
            if (p.id == paramId) {
                slot.page     = p.page;
                slot.paramCol = p.paramCol;
                slot.min      = p.min;
                slot.max      = p.max;
                break;
            }
        }
    }
    const juce::ScopedLock sl(listenerLock);
    ccMap_[(size_t)cc] = slot;
}

int MidiEngine::getCcMap(int cc) const
{
    if (cc < 0 || cc >= 128) return -1;
    const juce::ScopedLock sl(listenerLock);
    return ccMap_[(size_t)cc].paramId;
}

void MidiEngine::saveCcMap(juce::PropertiesFile& props) const
{
    juce::String val;
    const juce::ScopedLock sl(listenerLock);
    for (int i = 0; i < 128; ++i) {
        if (i > 0) val += ",";
        val += juce::String(ccMap_[(size_t)i].paramId);
    }
    props.setValue("ccMap", val);
}

void MidiEngine::loadCcMap(juce::PropertiesFile& props)
{
    juce::String saved = props.getValue("ccMap");
    if (saved.isEmpty()) return;
    auto tokens = juce::StringArray::fromTokens(saved, ",", "");
    for (int i = 0; i < juce::jmin(128, tokens.size()); ++i)
        setCcMap(i, tokens[i].trim().getIntValue());
}

// ---------------------------------------------------------------------------
// SysEx sub-handlers (called from handleIncomingMidiMessage — MIDI thread)
// getSysExData() layout (indices relative to the pointer passed as `data`):
//   [0]=0x10 (mfr)  [1]=deviceID  [2]=cmd  [3..]=payload
// ---------------------------------------------------------------------------

void MidiEngine::handlePageSelect(const uint8_t* data, juce::String& logMsg)
{
    currentRxPage    = data[3];
    currentRxSubPage = data[4];
    logMsg += juce::String::formatted(" [PageSelect page=0x%02x mode=%d]",
                                      (int)data[3], (int)data[4]);
}

// Parameter change: getSysExData=[10 id 0A 00 col 00 delta_lo delta_hi abs_lo abs_hi]
//   col < 0x18  → button/absolute: value at data[8]/data[9]
//   col >= 0x18 → rotary delta:    value at data[6]/data[7]; actual col = col - 0x10
//   Exception: col=0x1A on LFO pages subpage 1 = LFO_RETRIG_MODE (no subtract)
void MidiEngine::handleParameterChange(const uint8_t* data, int /*len*/, juce::String& logMsg)
{
    // Prefer a page-select from hardware; fall back to the last page XpandrLink sent.
    const int rxPage = currentRxPage.load();
    int effectivePage = (rxPage >= 0) ? rxPage : lastSentPage.load();
    if (effectivePage < 0) {
        logMsg += " [ParamChange DROPPED — page unknown; touch a parameter in the editor or press a page button on hardware]";
        return;
    }

    int paramCol = data[4];
    int val_lo, val_hi;
    bool isDelta = false;

    if (paramCol >= 0x18) {
        isDelta = true;
        val_lo  = data[6];
        val_hi  = data[7];
        // LFO retrig mode: col=0x1A on an LFO page (0x30-0x34) with button subpage — no subtract.
        bool isLfoRetrig = (paramCol == 0x1A)
                        && (effectivePage >= 0x30 && effectivePage <= 0x34)
                        && (currentRxSubPage == 1);
        if (!isLfoRetrig)
            paramCol -= 0x10;
    } else {
        val_lo = data[8];
        val_hi = data[9];
    }

    int value = ((val_hi & 0x01) != 0) ? -(0x80 - val_lo) : val_lo;
    logMsg += juce::String::formatted(" [Param page=0x%02x col=0x%02x val=%d%s]",
                                      effectivePage, paramCol, value,
                                      isDelta ? " delta" : "");

    const juce::ScopedLock sl(listenerLock);
    // Mirror the effectivePage fallback: use lastSentMode when hardware hasn't sent a
    // page-select yet (currentRxSubPage == -1).
    const int rxSubPage = currentRxSubPage.load();
    int effectiveSubPage = (rxSubPage >= 0) ? rxSubPage : lastSentMode.load();
    bool isButtonSubpage = (effectiveSubPage == 1);
    for (auto* l : listeners)
        l->onParameterChangedFromHardware(effectivePage, paramCol, value, isDelta, isButtonSubpage);
}

// Single patch dump: getSysExData=[10 id 01 00 progNum packed[0..391]], len=397
void MidiEngine::handlePatchDump(const uint8_t* data, int len, juce::String& logMsg)
{
    if (len == 397 && data[3] == 0x00) {
        const uint8_t* packed = data + 5;  // 392 packed bytes
        std::vector<uint8_t> raw(196);
        for (int i = 0; i < 196; ++i)
            raw[i] = unpackSysexByte(packed[2 * i], packed[2 * i + 1]);

        // Authoritative: sync currentProgram so subsequent nav SysEx deltas compute correctly.
        currentProgram = (int)data[4];

        const juce::ScopedLock sl(listenerLock);
        lastPatchSysexCache.clear();
        lastPatchSysexCache.push_back(0xF0);
        for (int i = 0; i < len; ++i) lastPatchSysexCache.push_back(data[i]);
        lastPatchSysexCache.push_back(0xF7);
        lastDecodedPatchCache = raw;
        for (auto* l : listeners) l->onPatchReceived(raw, (int)data[4]);
        logMsg += " → PATCH OK (prog=" + juce::String((int)data[4]) + ")";
    } else {
        logMsg += " [PATCH-FORMAT-MISMATCH: len=" + juce::String(len)
                + " expected=397, data[3]=0x" + juce::String::toHexString(data[3])
                + " expected=0x00]";
    }
}

// Modulation routing change from hardware (cmd=0x0F, front-panel mod-matrix edit).
// If we sent a mod command recently this is our own echo -- suppress it. This matters
// more than it used to: previously a spurious echo only triggered a wasteful (harmless)
// patch redump; now it would cause an actual double-apply of an incremental edit (e.g.
// our own DIAL +1 re-applied on top of the optimistic update we already made locally).
//
// Root-caused session 60 (see CLAUDE-MEMORY.md): a "Get Patch" dump request does NOT
// reflect front-panel mod-matrix edits on real hardware -- live capture proved the dump
// always returns stale mod-matrix bytes no matter how long you wait after an edit. So
// this decodes the edit message directly instead of requesting a resync dump, matching
// the C# reference's HandleModulationEditFromSynth (XpanderController.MIDIEvents.cs).
void MidiEngine::handleModRouting(const uint8_t* data, int len, juce::String& logMsg)
{
    logMsg += " [ModRouting]";
    auto elapsed = (juce::int32)(juce::Time::getMillisecondCounter() - lastModSentTime_);
    if (elapsed >= 0 && elapsed < 75) {
        logMsg += " [echo-suppressed]";
        return;
    }

    ModEdit edit;
    if (!decodeModulationEditMessage(data, len, currentRxPage.load(), currentRxSubPage.load(), edit)) {
        logMsg += " [undecodable — dest unknown or wrong length; dropped]";
        return;
    }
    logMsg += juce::String::formatted(" dest=%d slot=%d cmd=%d val=%d",
                                      edit.destIndex, edit.idSource, (int)edit.command, edit.value);

    const juce::ScopedLock sl(listenerLock);
    for (auto* l : listeners) l->onModulationEditFromHardware(edit);
}

// Front-panel patch navigation (+/- buttons):
// UP:   F0 10 [id] 0E 04 F7  DOWN: F0 10 [id] 0E 08 F7
void MidiEngine::handleProgramNav(const uint8_t* data, juce::String& logMsg)
{
    int delta = 0;
    if      (data[3] == 0x04) { delta = +1; logMsg += " ProgramChangeUP"; }
    else if (data[3] == 0x08) { delta = -1; logMsg += " ProgramChangeDOWN"; }
    if (delta != 0) {
        currentProgram = juce::jlimit(0, 99, currentProgram + delta);
        const juce::ScopedLock sl(listenerLock);
        for (auto* l : listeners) l->onProgramChangeReceived(currentProgram);
    }
}

// ---------------------------------------------------------------------------

uint8_t MidiEngine::unpackSysexByte(uint8_t lo, uint8_t hi)
{
    return (uint8_t)(((hi & 0x01) << 7) | (lo & 0x7F));
}

// Decode the two sign-magnitude bytes the Xpander/Matrix-12 uses for parameter values.
// val_hi==1 signals a negative value: value = -(0x80 - val_lo).
// val_hi==0: value = val_lo.
int MidiEngine::decodeSysexValue(uint8_t val_lo, uint8_t val_hi)
{
    return ((val_hi & 0x01) != 0) ? -(0x80 - (int)val_lo) : (int)val_lo;
}

bool MidiEngine::loadPatchFromFile(const juce::File& f)
{
    juce::MemoryBlock fileData;
    if (!f.loadFileAsData(fileData)) return false;

    return loadPatchFromMemory(static_cast<const uint8_t*>(fileData.getData()),
                               fileData.getSize());
}

bool MidiEngine::loadPatchFromMemory(const uint8_t* bytes, size_t sz)
{
    if (bytes == nullptr) return false;

    // A valid single-patch SysEx is exactly 399 bytes: F0 + 397 sysex bytes + F7.
    // Scan tolerantly in case the buffer has leading/trailing null padding.
    for (size_t i = 0; i + 399 <= sz; ++i)
    {
        if (bytes[i] != 0xF0) continue;
        if (bytes[i + 398] != 0xF7) continue;

        const uint8_t* d = bytes + i + 1;  // 397 bytes after F0
        // Validate: mfr=0x10, cmd=0x01, reserved=0x00
        if (d[0] != 0x10 || d[2] != 0x01 || d[3] != 0x00) continue;

        const uint8_t* packed = d + 5;  // 392 packed bytes
        std::vector<uint8_t> raw(196);
        for (int j = 0; j < 196; ++j)
            raw[j] = unpackSysexByte(packed[2 * j], packed[2 * j + 1]);

        const juce::ScopedLock sl(listenerLock);
        lastPatchSysexCache.assign(bytes + i, bytes + i + 399);
        lastDecodedPatchCache = raw;
        for (auto* l : listeners) l->onPatchReceived(raw, (int)d[4]);
        return true;
    }
    return false;
}

bool MidiEngine::loadInitPatch()
{
    return loadPatchFromMemory(reinterpret_cast<const uint8_t*>(BinaryData::InitPatch_syx),
                               (size_t)BinaryData::InitPatch_syxSize);
}

bool MidiEngine::hasCachedPatch()
{
    const juce::ScopedLock sl(listenerLock);
    return !lastPatchSysexCache.empty();
}

bool MidiEngine::savePatchToFile(const juce::File& f)
{
    const juce::ScopedLock sl(listenerLock);
    if (lastPatchSysexCache.empty()) return false;
    return f.replaceWithData(lastPatchSysexCache.data(), lastPatchSysexCache.size());
}

void MidiEngine::setCachedPatch(const std::vector<uint8_t>& sysex399)
{
    if (sysex399.size() != 399) return;
    const juce::ScopedLock sl(listenerLock);
    lastPatchSysexCache = sysex399;
}

void MidiEngine::broadcastCachedPatch()
{
    std::vector<uint8_t> raw;
    const juce::ScopedLock sl(listenerLock);
    // lastPatchSysexCache layout: [F0, 10, id, 01, 00, progNum, packed[0..391], F7]
    if (lastPatchSysexCache.size() != 399) return;
    const uint8_t* packed = lastPatchSysexCache.data() + 6;
    raw.resize(196);
    for (int i = 0; i < 196; ++i)
        raw[i] = unpackSysexByte(packed[2 * i], packed[2 * i + 1]);
    lastDecodedPatchCache = raw;
    int progNum = (int)lastPatchSysexCache[5];
    for (auto* l : listeners)
        l->onPatchReceived(raw, progNum);
}

void MidiEngine::sendProgramChange(int programNumber)
{
    if (activeOutput) {
        // Keep currentProgram in sync so subsequent nav SysEx deltas compute correctly.
        if (programNumber >= 0 && programNumber < 100)
            currentProgram = programNumber;
        // MIDI channel 1 (0x00 wire value); matches Xpander/Matrix-12 default channel
        activeOutput->sendMessageNow(juce::MidiMessage::programChange(1, programNumber));
        juce::String logMsg = "TX: ProgramChange prog=" + juce::String(programNumber);
        const juce::ScopedLock sl(listenerLock);
        for (auto* l : listeners) {
            l->onMidiActivity(false);
            DBG(logMsg);
            l->onStatusUpdate(logMsg);
        }
    }
}

void MidiEngine::requestPatchDump(int programNumber)
{
    // F0 10 [sysexID] 00 00 [progNum] F7
    std::vector<unsigned char> req = {
        0xF0, 0x10, (unsigned char)sysexID,
        0x00, 0x00, (unsigned char)programNumber,
        0xF7
    };
    sendSysex(req);
    juce::String logMsg = "TX: PatchDumpRequest id=" + juce::String((int)sysexID)
                        + " prog=" + juce::String(programNumber);
    const juce::ScopedLock sl(listenerLock);
    DBG(logMsg);
    for (auto* l : listeners) l->onStatusUpdate(logMsg);
}

void MidiEngine::sendPatchToSynth()
{
    // Copy the cache under the lock, then work outside the lock.
    std::vector<uint8_t> cache;
    {
        const juce::ScopedLock sl(listenerLock);
        cache = lastPatchSysexCache;
    }
    if (cache.size() != 399) return;

    // Update device-ID byte (index 2) to match the currently selected sysexID
    // before sending, in case the cache was loaded from a file with a different ID.
    cache[2] = (uint8_t)sysexID;

    // BUG-32: never overwrite the patch's own stored slot on the synth. Redirect the
    // outgoing dump (and the activating program-change) to a dedicated scratchpad slot
    // so loading/auditioning patches leaves all the user's other memory slots intact.
    // We mutate only this local copy — lastPatchSysexCache (and savePatchToFile) keep
    // the original program number.
    const int progNum = Oberheim::kScratchpadProgram;
    cache[5] = (uint8_t)progNum;

    juce::String logMsg = "TX: PatchToSynth id=" + juce::String((int)sysexID)
                        + " prog=" + juce::String(progNum) + " (scratchpad)";
    {
        const juce::ScopedLock sl(listenerLock);
        for (auto* l : listeners) {
            DBG(logMsg);
            l->onStatusUpdate(logMsg);
            l->onPatchSentToSynth(progNum);
        }
    }

    // C# reference: after the dump SysEx the hardware needs a Program Change
    // to select/activate the patch (mirrors SendFullToneToSynthIntoProgram).
    enqueueMessage(std::move(cache), Oberheim::kPatchSendSettleMs);
    enqueueAction([this, progNum] { sendProgramChange(progNum); }, 0);
}

void MidiEngine::sendPatchBytesToSlot(const std::vector<uint8_t>& sysex399, int targetSlot)
{
    if (sysex399.size() != 399) return;
    std::vector<uint8_t> data = sysex399;
    data[2] = (uint8_t)sysexID;
    int slot = juce::jlimit(0, 99, targetSlot);
    data[5] = (uint8_t)slot;
    enqueueMessage(std::move(data), Oberheim::kPatchSendSettleMs);
    // 150 ms delay after the program-change gives the hardware time to process
    // each dump before the next arrives (matches C# DELAY_BETWEEN_ALL_DATA_DUMP_SEND_SINGLE_PATCH).
    enqueueAction([this, slot] { sendProgramChange(slot); }, 150);
}

void MidiEngine::sendAllNotesOff()
{
    if (activeOutput)
        activeOutput->sendMessageNow(juce::MidiMessage::allNotesOff(1));
}

void MidiEngine::storePatchToSlot(int slot)
{
    slot = juce::jlimit(0, 99, slot);

    std::vector<uint8_t> cache;
    {
        const juce::ScopedLock sl(listenerLock);
        cache = lastPatchSysexCache;
    }
    if (cache.size() != 399) return;

    // Dump the patch stamped to the target slot and activate it via program-change
    // (sendPatchBytesToSlot already does both, 150 ms apart).
    sendPatchBytesToSlot(cache, slot);

    // STORE commit: permanently writes the just-dumped patch into non-volatile patch
    // memory at this slot (F0 10 [id] 07 [slot] F7, 6 bytes total — verified against
    // XpanderController.StoreSinglePatchToSynth). This command byte does NOT collide
    // with mod-matrix SETSIGN, whose top-level command byte is 0x0F, not 0x07 — see
    // the G2 note in ROADMAP.md.
    juce::String logMsg = "TX: Store id=" + juce::String((int)sysexID)
                        + " prog=" + juce::String(slot);
    {
        const juce::ScopedLock sl(listenerLock);
        DBG(logMsg);
        for (auto* l : listeners) l->onStatusUpdate(logMsg);
    }
    enqueueMessage({0xF0, 0x10, (uint8_t)sysexID, 0x07, (uint8_t)slot, 0xF7},
                   Oberheim::kPatchSendSettleMs);

    // Re-request the slot to confirm the store landed.
    enqueueAction([this, slot] { requestPatchDump(slot); }, 0);
}

void MidiEngine::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) {
    // Drop real-time system messages: they arrive at very high frequency (MIDI clock
    // at 48/sec at 120 BPM, active sensing every 300ms) and would flood the message
    // thread with callAsync calls, making the UI unresponsive and keeping the LED lit.
    if (message.isActiveSense() || message.isMidiClock() ||
        message.isMidiStart() || message.isMidiStop() || message.isMidiContinue())
        return;

    // Snapshot the designated synth input under the lock once per message — it can be
    // reassigned from the message thread (setSynthInput) while we run on the MIDI thread.
    juce::String synthIn;
    {
        const juce::ScopedLock sl(listenerLock);
        synthIn = synthInputName;
        for (auto* l : listeners) l->onMidiActivity(true);
    }

    // CC automation: intercept mapped CC from controller/DAW inputs before MIDI thru.
    // Does NOT send to synth — same path as hardware knob feedback.
    if (message.isController())
    {
        bool fromSynth = (source != nullptr && source->getName() == synthIn
                          && synthIn.isNotEmpty());
        if (!fromSynth)
        {
            int cc    = message.getControllerNumber();
            int ccVal = message.getControllerValue();
            CcMappedParam slot;
            { const juce::ScopedLock sl(listenerLock); slot = ccMap_[(size_t)cc]; }
            if (slot.paramId >= 0 && slot.page >= 0)
            {
                int range  = slot.max - slot.min;
                int scaled = (range == 1)
                           ? (ccVal > 63 ? 1 : 0)
                           : slot.min + (int)std::round(ccVal * (double)range / 127.0);
                scaled = juce::jlimit(slot.min, slot.max, scaled);
                broadcastParameterChange(slot.page, slot.paramCol, scaled, false, false);
                return;
            }
        }
    }

    // MIDI thru: forward non-SysEx from controller inputs (not the synth input) to the output.
    // Only active when a synth input is explicitly designated — safe default is no forwarding.
    if (!message.isSysEx() && synthIn.isNotEmpty())
    {
        bool fromSynth = (source != nullptr && source->getName() == synthIn);
        if (!fromSynth)
        {
            // Lock: setMidiOutput (message thread) may reset activeOutput concurrently.
            const juce::ScopedLock sl(listenerLock);
            if (activeOutput)
                activeOutput->sendMessageNow(message);
        }
    }

    juce::String logMsg = "RX: ";
    if (message.isSysEx()) {
        auto data = message.getSysExData();   // F0 excluded, F7 excluded
        int  len  = message.getSysExDataSize();
        logMsg += "SysEx [" + juce::String(len) + "] ";
        for (int i = 0; i < juce::jmin(len, 16); ++i)
            logMsg += juce::String::toHexString(data[i]) + " ";
        if (len > 16) logMsg += "...";

        if (len >= 3 && data[0] == 0x10) {
            // Auto-designate the first port that sends valid Oberheim SysEx as the synth input
            // (persisted routing setting — which port to treat as "the synth" for thru/CC gating).
            bool justDesignatedPort = false;
            if (synthIn.isEmpty() && source != nullptr)
            {
                synthIn = source->getName();
                const juce::ScopedLock sl(listenerLock);
                synthInputName = synthIn;
                justDesignatedPort = true;
            }

            // Live, non-persisted "seen this session" confirmation — separate from the
            // routing name above, which can be restored from a *previous* session's
            // settings even while the synth is powered off right now. Fires once per
            // session, the first time we actually see a real Oberheim message, so the
            // SYNTH LED reflects a live sighting instead of stale routing config.
            bool justConfirmedLive = false;
            {
                const juce::ScopedLock sl(listenerLock);
                if (!synthSeenThisSession) { synthSeenThisSession = true; justConfirmedLive = true; }
            }

            if (justDesignatedPort || justConfirmedLive)
            {
                const juce::ScopedLock sl(listenerLock);
                for (auto* l : listeners) l->onSynthInputDetected(synthIn);
            }

            // Auto-learn the hardware's configured device ID from the first Oberheim
            // SysEx we receive. The receive path doesn't filter by ID (so IN still works
            // when it's wrong), but outgoing SysEx embeds sysexID and is silently ignored
            // by the hardware unless it matches.
            int detectedId = (int)data[1];
            if (detectedId >= 0 && detectedId < 16 && detectedId != sysexID)
            {
                sysexID = detectedId;
                const juce::ScopedLock sl(listenerLock);
                for (auto* l : listeners) l->onSysexIDDetected(detectedId);
            }

            uint8_t cmd = data[2];
            if      (cmd == 0x0B && len >= 5)  handlePageSelect     (data,      logMsg);
            else if (cmd == 0x0A && len >= 10) handleParameterChange(data, len, logMsg);
            else if (cmd == 0x01)              handlePatchDump      (data, len, logMsg);
            else if (cmd == 0x0F)              handleModRouting     (data, len, logMsg);
            else if (cmd == 0x0E && len >= 4)  handleProgramNav     (data,      logMsg);
        }
    } else if (message.isProgramChange()) {
        int progNum = message.getProgramChangeNumber();
        logMsg += "ProgramChange prog=" + juce::String(progNum);
        // Absolute ground truth from the synth — always reset currentProgram and
        // notify listeners. Two dump requests for the same program (if both 0x0E
        // SysEx and a MIDI PC arrive for the same patch change) are harmless.
        // Matches XpanderController.MIDIEvents.cs synth-input PC handler.
        if (progNum >= 0 && progNum < 100) {
            currentProgram = progNum;
            const juce::ScopedLock sl(listenerLock);
            for (auto* l : listeners) l->onProgramChangeReceived(progNum);
        }
    } else {
        // Channel messages (note-on/off, CC, pitch bend, aftertouch) arrive at high
        // frequency when playing a controller and are not diagnostically useful.
        // Skip logging them to avoid flooding the message thread with callAsync work.
        return;
    }

    // Temporary diagnostic (session 60 investigation): the hidden logEditor has no
    // visible surface anymore (session 51 replaced it with the MIDI pane), so route
    // every logged RX line to the debug console too. Remove once the mod-routing
    // "editor doesn't see it" bug is root-caused.
    DBG(logMsg);

    {
        const juce::ScopedLock sl(listenerLock);
        for (auto* l : listeners) l->onStatusUpdate(logMsg);
    }
}

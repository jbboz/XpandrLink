#pragma once
#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>

class MidiEngine : public juce::MidiInputCallback,
                   public juce::ChangeListener,
                   private juce::Timer {
public:
    // Hardware front-panel mod-matrix edit commands (cmd=0x0F messages).
    // Matches C# EnumModulationEditCommands (XpanderConstants.cs).
    enum class ModEditCommand {
        AddSource = 0, DeleteSource = 1, ChangeSource = 2, SetUnsignedValue = 3,
        DialValueAmountOfChange = 4, SetQuantize = 5, ToggleQuantize = 6, SetSign = 7,
        Unknown = 0xFF
    };

    // A decoded hardware mod-matrix edit. destIndex is resolved from the current
    // page/subpage context (the message itself doesn't carry a destination).
    struct ModEdit {
        int           destIndex = -1;   // 0-46, resolved via destIndexForPageSubPage
        int           idSource  = -1;   // hardware slot 0-5
        ModEditCommand command  = ModEditCommand::Unknown;
        int           value     = 0;    // sign-decoded value field
    };

    class Listener {
    public:
        // Auto-removes from the engine if still registered — safe to destroy without calling removeListener().
        virtual ~Listener() {
            if (auto* e = midiEngineRef_.get())
                e->removeListener(this);
        }
        virtual void onMidiActivity(bool isInput) = 0;
        // rawPatchBytes: 196 decoded (unpacked) bytes in binary patch order.
        // progNum: the program slot number from the SysEx header (0-based; 99 = scratchpad).
        virtual void onPatchReceived(const std::vector<uint8_t>& rawPatchBytes, int progNum) = 0;
        // isDelta=true means value is a signed delta (rotary encoder); apply as currentValue+value.
        // isDelta=false means value is an absolute replacement.
        // isButton=true means the hardware sent this on subpage 1 (button mode).
        virtual void onParameterChangedFromHardware(int page, int paramCol, int value, bool isDelta, bool isButton) = 0;
        virtual void onStatusUpdate(const juce::String& msg) = 0;
        // Called when a MIDI Program Change is received from hardware (programNumber is 0-based)
        virtual void onProgramChangeReceived(int /*programNumber*/) {}
        // Called (on message thread) whenever sendParameterToSynth actually sends — not fired
        // when the call is blocked by hardwareUpdateMode. Use this to notify the host.
        virtual void onParameterSentToSynth(int /*page*/, int /*paramCol*/, int /*value*/, bool /*isButton*/) {}
        // Called when the MIDI output device changes (empty string = none).
        virtual void onMidiOutputChanged(const juce::String& /*name*/) {}
        // Called (on the MIDI thread) the first time a valid Oberheim SysEx is received
        // from a previously-unknown source. The port is auto-designated as the synth input.
        virtual void onSynthInputDetected(const juce::String& /*portName*/) {}
        // Called (on the MIDI thread) the first time an incoming Oberheim SysEx carries
        // a device-ID byte different from our current sysexID. Auto-learns the hardware's
        // configured ID so outgoing SysEx isn't silently ignored by a mismatched device.
        virtual void onSysexIDDetected(int /*id*/) {}
        // Called (on the MIDI thread) when the hardware reports a front-panel mod-matrix
        // edit (cmd=0x0F), already decoded. The listener applies it incrementally to its
        // own mod-matrix model on the message thread. Does NOT imply a patch dump is
        // needed -- a "Get Patch" dump does not reflect live front-panel mod-matrix edits
        // on real hardware.
        virtual void onModulationEditFromHardware(const MidiEngine::ModEdit& /*edit*/) {}
        // Called (on message thread) after sendPatchToSynth() actually delivers the cached
        // patch to hardware. programNumber is the real destination slot (always the
        // scratchpad, Oberheim::kScratchpadProgram) — NOT necessarily the program number
        // embedded in the patch's SysEx header, which may reflect a bank/file origin.
        // Listener should update any displayed patch-number UI to this value so it doesn't
        // show a slot the patch was never actually written to.
        virtual void onPatchSentToSynth(int /*programNumber*/) {}

    private:
        friend class MidiEngine;
        juce::WeakReference<MidiEngine> midiEngineRef_;
    };

    MidiEngine();
    ~MidiEngine() override;

    // In standalone builds, share the host's AudioDeviceManager so that the
    // Options > Audio/MIDI Settings dialog controls XpandrLink's MIDI routing.
    void useDeviceManager(juce::AudioDeviceManager& dm);
    juce::AudioDeviceManager& getDeviceManager();
    bool isUsingExternalDeviceManager() const { return externalDeviceManager != nullptr; }

    // setMidiInput: exclusive (disables all others). addMidiInput: additive.
    void setMidiInput(const juce::String& deviceName);
    void addMidiInput(const juce::String& deviceName);
    void removeMidiInput(const juce::String& deviceName);
    juce::StringArray getEnabledMidiInputNames() const;

    // Designates which active input is the synth (Matrix-12). Messages received from
    // this port are processed for SysEx/parameter changes but never forwarded to the output.
    // Messages from all OTHER active inputs are forwarded as MIDI thru (notes, CC, etc.).
    // If empty, no forwarding happens (safe default).
    void setSynthInput(const juce::String& deviceName);
    juce::String getSynthInputName() const;   // guarded — safe from any thread

    // True once a genuine Oberheim SysEx has actually been received in THIS run of the
    // app. Deliberately NOT persisted and NOT the same thing as getSynthInputName(),
    // which is a routing setting restored from a previous session's settings file — a
    // MIDI port name stays "available" in the OS regardless of whether the synth
    // plugged into it is powered on, so getSynthInputName().isNotEmpty() alone is not
    // proof of a live connection. Used to drive the SYNTH presence LED honestly.
    bool hasSeenSynthThisSession() const;   // guarded — safe from any thread

    void setMidiOutput(const juce::String& deviceName);
    void setSysexID(int id);
    void setCurrentProgram(int p) { currentProgram = juce::jlimit(0, 99, p); }
    int  getCurrentProgram() const { return currentProgram; }

    // Manual synth-model flag (Xpander vs Matrix-12). Cannot be auto-detected: both
    // models transmit the same SysEx device-ID byte (see SPEC.md), so this is a
    // user-set switch, not learned from incoming SysEx.
    // Selects the model-dependent command byte needed by features like the display
    // banner (G1) and the all-data-dump request (G3).
    void setSynthTypeIsMatrix12(bool isMatrix12) { synthTypeIsMatrix12 = isMatrix12; }
    bool isSynthTypeMatrix12() const { return synthTypeIsMatrix12; }

    // Set while applying hardware-initiated updates so onValueChange callbacks
    // don't re-send the parameter back to the synth and cause a double-increment.
    // Set only on the message thread; read from the message thread and (by the
    // AudioProcessor's parameterValueChanged) the audio thread — hence atomic.
    void setHardwareUpdateMode(bool active) { hardwareUpdateMode = active; }
    bool isInHardwareUpdateMode() const { return hardwareUpdateMode; }

    void sendPageSelect(int page, int mode, bool force = false);
    void sendParameterToSynth(int page, int paramID, int absoluteValue, bool isButton = false);
    void addModulation(int sourceIndex, int destIndex, int amount, int idSourceForAmtAndSign);
    void removeModulation(int sourceIndex, int destIndex, int idSource);
    void changeModulationAmount(int destIndex, int idSource, int newAmount);

    // Atomic in-place source swap for an EXISTING mod-matrix entry (CHANGESOURCE, cmd=0x02).
    // Never touches the entry's hardware slot (idSource) -- unlike delete+re-add, there is
    // no risk of mispredicting which slot the hardware will assign the new source to.
    void changeModulationSource(int destIndex, int idSource, int newSourceIndex);

    // Public for unit testing: exact CHANGESOURCE byte layout, matches the C# reference
    // (XpanderTone.ModulationMatrix.cs, EnumModulationEditCommands.CHANGESOURCE=0x02).
    static std::vector<uint8_t> buildChangeSourceMessage(uint8_t sysexId, uint8_t idSource, uint8_t newSource);
    void sendProgramChange(int programNumber);
    void requestPatchDump(int programNumber = 0);

    // Load a .syx file into the editor (parses locally, calls onPatchReceived on all listeners).
    // Returns false if the file cannot be found or contains no valid single-patch SysEx.
    bool loadPatchFromFile(const juce::File& f);

    // Parse a raw byte buffer for a 399-byte single-patch SysEx and broadcast it
    // to all listeners (same path as loadPatchFromFile). Returns false if the
    // buffer contains no valid single patch.
    bool loadPatchFromMemory(const uint8_t* bytes, size_t size);

    // Load the Oberheim init patch embedded in the binary (BinaryData::InitPatch_syx).
    // Returns false only if the embedded asset is not a valid single patch.
    bool loadInitPatch();

    // True once at least one patch dump has been received or loaded from file.
    bool hasCachedPatch();

    // Write the last received/loaded patch to a .syx file.
    // Returns false if no patch has been cached yet.
    bool savePatchToFile(const juce::File& f);

    // Replace the cached SysEx with a freshly-encoded version from the editor UI.
    // Call this just before saving so the file reflects the current parameter state.
    void setCachedPatch(const std::vector<uint8_t>& sysex399);

    // Return a copy of the cached 399-byte SysEx, or empty if no patch has been cached.
    std::vector<uint8_t> getCachedPatch() const { return lastPatchSysexCache; }

    // Decode the cached SysEx and fire onPatchReceived on all current listeners.
    // Call this to push a restored/loaded patch into any open editor UI.
    void broadcastCachedPatch();

    bool isMidiOutputOpen() const { return activeOutput != nullptr; }

    // Send the cached 399-byte patch SysEx directly to the synth output.
    // The device-ID byte is updated to match the current sysexID before sending.
    // No-op if the cache is empty or no MIDI output is open.
    void sendPatchToSynth();

    // Send a raw 399-byte patch SysEx to a specific program slot on the synth.
    // Updates device-ID byte; does NOT redirect to scratchpad or touch lastPatchSysexCache.
    // Enqueues the dump + a program-change with 150 ms inter-patch spacing after the PC.
    // Intended for bank restore — normal audition/load should use sendPatchToSynth().
    void sendPatchBytesToSlot(const std::vector<uint8_t>& sysex399, int targetSlot);

    // Send an all-notes-off message to the synth output immediately (bypasses the queue).
    // Call this before starting a bank send to silence any held notes.
    void sendAllNotesOff();

    // PERMANENTLY commits the cached patch to a real hardware program slot (0-99).
    // Unlike sendPatchToSynth() (which always redirects to the scratchpad slot,
    // Oberheim::kScratchpadProgram, so ordinary load/audition/randomize/morph never
    // touches the user's other patches), this is the one deliberate, explicit exception:
    // it dumps the patch stamped to `slot`, activates it, then sends the STORE commit
    // (F0 10 [id] 07 [slot] F7) that writes it into non-volatile patch memory at that
    // slot, then re-requests the slot to confirm. No-op if no patch is cached.
    // Callers must confirm with the user before calling this — it overwrites real
    // hardware memory with no undo.
    void storePatchToSlot(int slot);

    // Master command buttons (nav bar).
    void sendMidiMute();   // Master -> MIDI -> Mute  (page-select 0B 1B 00)
    void sendTuneAll();    // Master -> Tune -> All   (MIDI Tune Request F6 = "tune all")

    // Write text to the synth's own front-panel display. Text is uppercased and
    // truncated/space-padded to exactly 80 chars (the hardware's fixed message length).
    // Always sends OFF -> ON -> text (matches the C# reference's SendGreetingsToSynth /
    // SendTypeWriterMessageToSynth). typewriter=true sends growing left-substrings of
    // the text (each still padded to 80 chars) at kDisplayScrollMs apart, for a
    // scrolling effect; typewriter=false sends the full text once.
    // Command byte depends on setSynthTypeIsMatrix12() (0x05 Xpander / 0x06 Matrix-12).
    void sendDisplayMessage(const juce::String& text, bool typewriter = false);

    // Turn the synth's display off/on without changing its text. Exposed separately
    // for a future greeting-on-connect; sendDisplayMessage() already calls both.
    void sendDisplayOff();
    void sendDisplayOn();

    int getSysexID() const { return sysexID; }
    juce::String getCurrentMidiInputName() const;
    juce::String getCurrentMidiOutputName() const;

    // Fire onParameterChangedFromHardware on all listeners without going through
    // the MIDI thread. Used by the AudioProcessor to push host-automation values
    // to the editor UI. Iterates under listenerLock.
    void broadcastParameterChange(int page, int paramCol, int value, bool isDelta, bool isButton = false);

    // Public for unit testing: decode the Oberheim sign-magnitude 2-byte encoding.
    static int decodeSysexValue(uint8_t val_lo, uint8_t val_hi);

    // Public for unit testing: reverse lookup of the page/subpage -> mod-destination
    // table (kModDestTable in MidiEngine.cpp). Returns 0-46, or -1 if (page, subPage)
    // isn't a known modulation destination.
    static int destIndexForPageSubPage(int page, int subPage);

    // Public for unit testing: decode a hardware front-panel mod-matrix edit SysEx
    // (cmd=0x0F). `data`/`len` are the JUCE getSysExData()/getSysExDataSize() form
    // (F0/F7 excluded). rxPage/rxSubPage are the caller's current page-select context
    // (currentRxPage/currentRxSubPage), used to resolve the destination since it isn't
    // carried in the message itself. Returns false if len is too short or the
    // destination can't be resolved; `out` is left unmodified in that case.
    static bool decodeModulationEditMessage(const uint8_t* data, int len,
                                            int rxPage, int rxSubPage, ModEdit& out);

    // Public for unit testing: true if `now` is still within `throttleMs` of `lastSendTime`
    // (i.e. a mod-matrix amount change should be coalesced/deferred rather than sent
    // immediately). Wraparound-safe signed subtraction, same pattern as handleModRouting.
    static bool shouldCoalesceModAmount(juce::uint32 now, juce::uint32 lastSendTime, int throttleMs);

    // Public for unit testing: picks a MIDI output to auto-select once the synth's input
    // port is detected, when no output is set yet. Only guesses when there's a single,
    // unambiguous candidate -- an output sharing the detected input's name (the common
    // case: an interface exposes matching names for its IN/OUT pair of a given port), or
    // the only output that exists at all. Returns an empty string when the choice would
    // be a guess (multiple outputs, none name-matching), since sending SysEx to the wrong
    // device is worse than asking the user to pick.
    static juce::String chooseAutoOutput(const juce::String& detectedInputName,
                                          const juce::StringArray& availableOutputs);

    // CC automation table: map CC numbers (0-127) to Xpander parameter IDs.
    // Incoming CC from non-synth inputs is scaled to the param range and broadcast
    // as a parameter-from-hardware event (does NOT send to synth — same as HW knob path).
    // Pass paramId=-1 to unmap.
    void setCcMap(int cc, int paramId);
    int  getCcMap(int cc) const;                        // returns paramId or -1 if unmapped
    void saveCcMap(juce::PropertiesFile& props) const;
    void loadCcMap(juce::PropertiesFile& props);

    // All listener notifications iterate while holding listenerLock (never snapshot-then-call).
    // CriticalSection is recursive, so listener callbacks that re-enter MidiEngine are safe.
    void addListener(Listener* l);
    void removeListener(Listener* l);

    juce::StringArray getMidiInputs() const;
    juce::StringArray getMidiOutputs() const;

    JUCE_DECLARE_WEAK_REFERENCEABLE(MidiEngine)

private:
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    // Sub-handlers called from handleIncomingMidiMessage (each appends to logMsg)
    void handlePageSelect     (const uint8_t* data,               juce::String& logMsg);
    void handleParameterChange(const uint8_t* data, int len,      juce::String& logMsg);
    void handlePatchDump      (const uint8_t* data, int len,      juce::String& logMsg);
    void handleModRouting     (const uint8_t* data, int len,      juce::String& logMsg);
    void handleProgramNav     (const uint8_t* data,               juce::String& logMsg);
    void syncMidiInputCallbacks();
    void syncMidiOutputFromDeviceManager();
    juce::AudioDeviceManager& activeManager();
    void sendSysex(const std::vector<unsigned char>& data);

    // Send-queue helpers — all called on the message thread only.
    // Enqueue a SysEx message (incl. F0/F7); it will be sent after the current settle period.
    void enqueueMessage(std::vector<uint8_t> bytes, int delayAfterMs = 0);
    // Enqueue a non-SysEx action (e.g. sendProgramChange) with an optional delay before it runs.
    void enqueueAction(std::function<void()> action, int delayAfterMs = 0);
    // Enqueue a page-select unless the queued page already matches. force=true always enqueues.
    void enqueuePageSelectIfNeeded(int page, int mode, int settleMs, bool force = false);

    // Send-queue state (message-thread only — no lock needed).
    struct QueuedMessage {
        std::vector<uint8_t>    bytes;         // SysEx (incl. F0/F7); empty → use action
        std::function<void()>   action;
        int                     delayAfterMs = 0;
    };
    std::deque<QueuedMessage> sendQueue;
    juce::uint32              nextSendTime   = 0;
    int                       lastQueuedPage = -1;  // page last pushed onto sendQueue
    int                       lastQueuedMode = -1;

    juce::AudioDeviceManager  deviceManager;
    juce::AudioDeviceManager* externalDeviceManager = nullptr;
    // Reset/reopened on the message thread; dereferenced on the MIDI thread (thru
    // forward). Both sides take listenerLock around access.
    std::unique_ptr<juce::MidiOutput> activeOutput;

    // Cross-thread scalars: written on the MIDI thread (auto-detect, rx page tracking)
    // and/or message thread (setters, queue drain), read from the other side.
    std::atomic<int> sysexID { 2 };
    std::atomic<bool> synthTypeIsMatrix12 { false };
    juce::String synthInputName;  // designated synth input; guarded by listenerLock
    bool synthSeenThisSession = false;  // live confirmation, not persisted; guarded by listenerLock
    std::atomic<int> lastSentPage { -1 };  // page last actually sent to the synth (updated at drain time)
    std::atomic<int> lastSentMode { -1 };
    std::atomic<int> currentRxPage    { -1 };
    std::atomic<int> currentRxSubPage { -1 };
    std::atomic<int> currentProgram   { 0 };  // tracks hardware program number; updated by ProgramChange and nav SysEx
    std::atomic<bool> hardwareUpdateMode { false };  // set on message thread; blocks sendParameterToSynth during hw updates
    std::atomic<juce::uint32> lastModSentTime_ { 0 }; // ms counter updated when a 0x0F message is drained; suppresses echoes

    // Rapid drag coalescing for mod-matrix amount changes (message-thread only, same as
    // the rest of the send-queue machinery). A fast slider drag can fire changeModulationAmount
    // dozens of times a second; since SETSIGN+SETUNSIGNEDVALUE have zero delay between them
    // (deliberately -- see changeModulationAmount), timerCallback's drain-all-zero-delay-messages
    // loop would flood the synth's MIDI IN far faster than its UART/firmware can keep up,
    // observed as a temporary hardware lockup that self-recovers once the flood stops
    // (found 2026-07-13). Only the LATEST value during a throttle window is ever sent.
    struct PendingModAmount { bool valid = false; int destIndex = -1; int idSource = -1; int amount = 0; };
    PendingModAmount pendingModAmount_;
    juce::uint32 lastModAmountSendTime_ { 0 };
    void sendModAmountNow(int destIndex, int idSource, int newAmount);

    // CC automation map — accessed from both message thread (write) and MIDI thread (read).
    // Protected by listenerLock. paramId=-1 means unmapped.
    struct CcMappedParam {
        int paramId  = -1;
        int page     = -1;
        int paramCol = -1;
        int min      =  0;
        int max      = 127;
    };
    std::array<CcMappedParam, 128> ccMap_;

    mutable juce::CriticalSection listenerLock;
    std::vector<Listener*> listeners;

    static uint8_t unpackSysexByte(uint8_t lo, uint8_t hi);

    // Cached as complete SysEx (F0 ... F7), updated on every patch dump received or loaded from file.
    std::vector<uint8_t> lastPatchSysexCache;
    // Decoded 196-byte form of lastPatchSysexCache — kept in sync for fast replay to new listeners.
    std::vector<uint8_t> lastDecodedPatchCache;
};
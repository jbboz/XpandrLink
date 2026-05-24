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
        // Called when the hardware sends any modulation-routing SysEx (cmd=0x0F).
        // Listener should request a patch dump to refresh the mod matrix panel.
        virtual void onModulationRoutingChangedByHardware() {}
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

    void setMidiOutput(const juce::String& deviceName);
    void setSysexID(int id);
    void setCurrentProgram(int p) { currentProgram = juce::jlimit(0, 99, p); }
    int  getCurrentProgram() const { return currentProgram; }

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

    // Master command buttons (nav bar).
    void sendMidiMute();   // Master -> MIDI -> Mute  (page-select 0B 1B 00)
    void sendTuneAll();    // Master -> Tune -> All   (MIDI Tune Request F6 = "tune all")

    int getSysexID() const { return sysexID; }
    juce::String getCurrentMidiInputName() const;
    juce::String getCurrentMidiOutputName() const;

    // Fire onParameterChangedFromHardware on all listeners without going through
    // the MIDI thread. Used by the AudioProcessor to push host-automation values
    // to the editor UI. Iterates under listenerLock.
    void broadcastParameterChange(int page, int paramCol, int value, bool isDelta, bool isButton = false);

    // Public for unit testing: decode the Oberheim sign-magnitude 2-byte encoding.
    static int decodeSysexValue(uint8_t val_lo, uint8_t val_hi);

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
    void handleModRouting     (                                    juce::String& logMsg);
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
    juce::String synthInputName;  // designated synth input; guarded by listenerLock
    std::atomic<int> lastSentPage { -1 };  // page last actually sent to the synth (updated at drain time)
    std::atomic<int> lastSentMode { -1 };
    std::atomic<int> currentRxPage    { -1 };
    std::atomic<int> currentRxSubPage { -1 };
    std::atomic<int> currentProgram   { 0 };  // tracks hardware program number; updated by ProgramChange and nav SysEx
    std::atomic<bool> hardwareUpdateMode { false };  // set on message thread; blocks sendParameterToSynth during hw updates
    std::atomic<juce::uint32> lastModSentTime_ { 0 }; // ms counter updated when a 0x0F message is drained; suppresses echoes

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
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <deque>
#include <functional>
#include <cstdint>

// Generic MIDI send-queue / pacing machinery, extracted verbatim from MidiEngine
// (Phase 4, code-quality plan). Protocol-agnostic: it stores queued SysEx byte-vectors
// and deferred actions and drains them on a caller-driven clock, pacing by each message's
// delayAfterMs. It knows nothing about the MIDI backend, the Oberheim protocol, or
// MidiEngine's cross-thread atomics -- the one hook back into MidiEngine is onSendSysex.
//
// Threading: message-thread only, exactly as when this lived inline in MidiEngine
// (no lock needed). The pacing/drain design and its rationale are governed by
// ADR-003 (docs/adr/003-timer-pacing-coalescing.md); the mod-amount coalescing that
// ADR also covers deliberately stays on MidiEngine, not here.
class MidiSendQueue {
public:
    // Enqueue a SysEx message (incl. F0/F7); it will be sent after the current settle period.
    void enqueueMessage(std::vector<uint8_t> bytes, int delayAfterMs = 0);
    // Enqueue a non-SysEx action (e.g. sendProgramChange) with an optional delay before it runs.
    void enqueueAction(std::function<void()> action, int delayAfterMs = 0);
    // Enqueue a page-select unless the queued page already matches. force=true always enqueues.
    // sysexId is supplied by the caller (MidiEngine's atomic) since this class is protocol-agnostic
    // and does not own the device ID.
    void enqueuePageSelectIfNeeded(int page, int mode, int settleMs, uint8_t sysexId, bool force = false);

    // Drain due messages given the caller's shared `now` snapshot (ms counter). Sends every
    // run of zero-delay messages back-to-back, then stops after the first message carrying a
    // nonzero delay (settling before the next). Signed subtraction is used against `now` to
    // stay correct across the uint32 ms-counter wraparound (~49-day period).
    void drain(juce::uint32 now);

    // Clear the queue and the queued-page dedup state. Does NOT touch nextSendTime_ --
    // matches the historical setMidiOutput/destructor behavior exactly.
    void reset();

    // Bridge back to MidiEngine: invoked for each drained SysEx message with the shared
    // `now` snapshot. MidiEngine wires this (once, in its constructor) to sendSysex() plus
    // the byte-inspection that updates lastSentPage/lastSentMode/lastModSentTime_. `now` is
    // passed through (not recomputed here) so lastModSentTime_ tracks the drain's own clock
    // exactly, preserving the mod-echo suppression window's timing.
    std::function<void(const std::vector<uint8_t>& bytes, juce::uint32 now)> onSendSysex;

private:
    struct QueuedMessage {
        std::vector<uint8_t>    bytes;         // SysEx (incl. F0/F7); empty -> use action
        std::function<void()>   action;
        int                     delayAfterMs = 0;
    };
    std::deque<QueuedMessage> queue_;
    juce::uint32              nextSendTime_   = 0;
    int                       lastQueuedPage_ = -1;  // page last pushed onto queue_
    int                       lastQueuedMode_ = -1;
};

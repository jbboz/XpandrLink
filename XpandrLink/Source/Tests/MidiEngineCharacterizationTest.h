/*
  MidiEngineCharacterizationTest.h

  Code-quality plan Phase 2.5 (docs/plans/2026-07-16-code-quality-improvement-plan.md):
  a byte-for-byte regression oracle for MidiEngine::handleIncomingMidiMessage's actual
  dispatch logic, driven end-to-end through the real private handler (via the new
  processIncomingMessageForTest() seam) rather than a hand-copied mirror of the decode
  rules. Existing tests (BUG05, ModEditDecodeTest, BUG01) already cover several of the
  underlying pure/extracted helpers in isolation; this file exercises the *integration*
  those tests don't reach: page/subpage-context tracking feeding into the parameter-change
  fallback logic, the LFO-retrig subtract exception, patch-dump decode via the real
  handler, mod-edit destination resolution from live page-select state, sysexID
  auto-learn, program-change reconciliation, and CC-automation interception — all
  observed purely through MidiEngine::Listener callbacks (no MockMidiBackend/IMidiBackend
  needed for this; see the plan's Phase 3 for that).

  Deliberately NOT covered by this seam (documented gap, not an oversight): the
  synth-input-auto-detect and MIDI-thru-forward branches both depend on
  `source->getName()` for a real, named juce::MidiInput, which this seam (source may be
  nullptr) does not attempt to fabricate. Also not covered: any outgoing byte-for-byte
  send-path capture -- see MidiEngineSendPathTest.h (Phase 3), which drives the real send/
  queue-drain path through a MockMidiBackend, including the burst-drag mod-amount
  coalescing scenario (the mod-matrix echo-suppression window, lastModSentTime_, remains
  untested end-to-end either way -- it needs a live Timer/message-loop pump that neither
  phase's seam provides).

  Verifies: NFR-HW-03, NFR-HW-05, NFR-HW-07, NFR-HW-08, NFR-HW-09, NFR-HW-10 (see docs/NFR.md).
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "../Engine/SynthDefs.h"
#include "../Data/XpanderData.h"

class MidiEngineCharacterizationTest : public juce::UnitTest
{
public:
    MidiEngineCharacterizationTest() : juce::UnitTest("MidiEngine characterization", "MidiEngine") {}

    // Captures every MidiEngine::Listener callback relevant to this file's tests.
    struct CaptureListener : public MidiEngine::Listener
    {
        int paramCalls = 0;
        int lastPage = -1, lastParamCol = -1, lastValue = 0;
        bool lastIsDelta = false, lastIsButton = false;

        int patchCalls = 0;
        std::vector<uint8_t> lastRaw;
        int lastProgNum = -1;

        int modEditCalls = 0;
        MidiEngine::ModEdit lastModEdit;

        int progChangeCalls = 0;
        int lastProgChange = -1;

        int sysexIdCalls = 0;
        int lastSysexId = -1;

        juce::String lastStatus;
        std::vector<juce::String> allStatus;

        void onMidiActivity(bool) override {}
        void onPatchReceived(const std::vector<uint8_t>& raw, int progNum) override
        {
            ++patchCalls; lastRaw = raw; lastProgNum = progNum;
        }
        void onParameterChangedFromHardware(int page, int paramCol, int value, bool isDelta, bool isButton) override
        {
            ++paramCalls;
            lastPage = page; lastParamCol = paramCol; lastValue = value;
            lastIsDelta = isDelta; lastIsButton = isButton;
        }
        void onStatusUpdate(const juce::String& msg) override { lastStatus = msg; allStatus.push_back(msg); }
        void onProgramChangeReceived(int programNumber) override { ++progChangeCalls; lastProgChange = programNumber; }
        void onModulationEditFromHardware(const MidiEngine::ModEdit& edit) override { ++modEditCalls; lastModEdit = edit; }
        void onSysexIDDetected(int id) override { ++sysexIdCalls; lastSysexId = id; }
    };

    // Builds a real juce::MidiMessage the same way sendSysex() does: `bytes` excludes F0/F7.
    static juce::MidiMessage sysex(std::initializer_list<uint8_t> bytes)
    {
        std::vector<uint8_t> data(bytes);
        return juce::MidiMessage::createSysExMessage(data.data(), (int)data.size());
    }

    void runTest() override
    {
        // -----------------------------------------------------------------
        // Page-select + button parameter change
        // -----------------------------------------------------------------
        beginTest("Page-select then button (subpage-1) parameter change resolves via RX page context");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            // F0 10 02 0B 22 01 F7 -> page=0x22 (VCF), mode=1 (button subpage)
            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0B, 0x22, 0x01 }));
            // F0 10 02 0A 00 05 00 00 00 2A F7 -> col=0x05 (<0x18, absolute), val_lo=0x2A val_hi=0
            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0A, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2A, 0x00 }));

            expectEquals(l.paramCalls, 1);
            expectEquals(l.lastPage, 0x22);
            expectEquals(l.lastParamCol, 0x05);
            expectEquals(l.lastValue, 0x2A);
            expect(!l.lastIsDelta, "absolute (col<0x18) is not a delta");
            expect(l.lastIsButton, "subpage 1 -> isButton true");

            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // Rotary knob delta (col >= 0x18)
        // -----------------------------------------------------------------
        beginTest("Rotary knob delta (col>=0x18) subtracts 0x10 and reads signed value from data[6]/[7]");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0B, 0x22, 0x00 })); // slider subpage
            // col=0x20, delta val_lo=0x05 val_hi=0 -> +5
            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0A, 0x00, 0x20, 0x00, 0x05, 0x00, 0x00, 0x00 }));

            expectEquals(l.paramCalls, 1);
            expectEquals(l.lastParamCol, 0x20 - 0x10);
            expectEquals(l.lastValue, 5);
            expect(l.lastIsDelta, "col>=0x18 is a delta");
            expect(!l.lastIsButton, "slider subpage -> isButton false");

            engine.removeListener(&l);
        }

        beginTest("Rotary knob delta with sign bit set decodes as negative");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0B, 0x22, 0x00 }));
            // val_lo=0x7E val_hi=1 -> -(0x80-0x7E) = -2
            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0A, 0x00, 0x20, 0x00, 0x7E, 0x01, 0x00, 0x00 }));

            expectEquals(l.lastValue, -2);
            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // LFO retrig-mode exception
        // -----------------------------------------------------------------
        beginTest("col=0x1A on an LFO page's button subpage skips the 0x10 subtract (LFO_RETRIG_MODE)");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            // PAGE_LFO1 = 0x30, mode=1 (button subpage) -- both conditions of the exception.
            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0B, (uint8_t)Matrix12::PAGE_LFO1, 0x01 }));
            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0A, 0x00, 0x1A, 0x00, 0x02, 0x00, 0x00, 0x00 }));

            expectEquals(l.paramCalls, 1);
            expectEquals(l.lastParamCol, 0x1A, "no subtract for the LFO retrig exception");
            expectEquals(l.lastValue, 2);
            expect(l.lastIsDelta, "col>=0x18 is still flagged as a delta even for the exception");

            engine.removeListener(&l);
        }

        beginTest("col=0x1A on a non-LFO page is an ordinary delta (0x10 IS subtracted)");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0B, 0x22, 0x01 })); // VCF, not LFO
            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0A, 0x00, 0x1A, 0x00, 0x02, 0x00, 0x00, 0x00 }));

            expectEquals(l.lastParamCol, 0x1A - 0x10, "not the LFO page -> ordinary subtract applies");
            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // Page-unknown drop
        // -----------------------------------------------------------------
        beginTest("Parameter change with no page context yet (fresh engine) is dropped, not misattributed");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            // No prior page-select and nothing ever sent -> currentRxPage and
            // lastSentPage are both -1.
            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0A, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2A, 0x00 }));

            expectEquals(l.paramCalls, 0);
            expect(l.lastStatus.contains("DROPPED"), "expected a DROPPED status message");

            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // Patch dump
        // -----------------------------------------------------------------
        beginTest("Patch dump (cmd=0x01) decodes 196 raw bytes and reports the header's program number");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            std::vector<uint8_t> raw(196);
            for (int i = 0; i < 196; ++i) raw[(size_t)i] = (uint8_t)(i * 37 + 11);

            const int progNum = 42;
            std::vector<uint8_t> data;
            data.push_back(0x10); data.push_back(0x02); data.push_back(0x01);
            data.push_back(0x00); data.push_back((uint8_t)progNum);
            for (int i = 0; i < 196; ++i) {
                data.push_back((uint8_t)(raw[(size_t)i] & 0x7F));
                data.push_back((uint8_t)((raw[(size_t)i] >> 7) & 0x01));
            }
            expectEquals((int)data.size(), 397);

            engine.processIncomingMessageForTest(nullptr,
                juce::MidiMessage::createSysExMessage(data.data(), (int)data.size()));

            expectEquals(l.patchCalls, 1);
            expectEquals(l.lastProgNum, progNum);
            expect(l.lastRaw == raw, "decoded raw bytes must round-trip exactly");
            expectEquals(engine.getCurrentProgram(), progNum);

            auto cached = engine.getCachedPatch();
            expectEquals((int)cached.size(), 399);
            expectEquals((int)cached.front(), 0xF0);
            expectEquals((int)cached.back(), 0xF7);

            engine.removeListener(&l);
        }

        beginTest("Malformed patch dump (wrong length) is rejected, no listener call");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            std::vector<uint8_t> data = { 0x10, 0x02, 0x01, 0x00, 0x05, 0x00, 0x00 }; // way too short
            engine.processIncomingMessageForTest(nullptr,
                juce::MidiMessage::createSysExMessage(data.data(), (int)data.size()));

            expectEquals(l.patchCalls, 0);
            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // Mod-matrix edit (cmd=0x0F) destination resolution
        // -----------------------------------------------------------------
        beginTest("Mod-matrix edit resolves destIndex from the RX page/subpage context set by a prior page-select");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            // page=0x22 mode=6 -> VCA1 Vol (destIndex 8), matching ModEditDecodeTest's
            // live-captured example.
            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0B, 0x22, 0x06 }));
            // ADDSOURCE, source=0x13 (LFO1)
            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00 }));

            expectEquals(l.modEditCalls, 1);
            expectEquals(l.lastModEdit.destIndex, 8);
            expectEquals(l.lastModEdit.idSource, 0);
            expect(l.lastModEdit.command == MidiEngine::ModEditCommand::AddSource);
            expectEquals(l.lastModEdit.value, 0x13);

            engine.removeListener(&l);
        }

        beginTest("Mod-matrix edit with no prior page-select context is undecodable and dropped");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            engine.processIncomingMessageForTest(nullptr,
                sysex({ 0x10, 0x02, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00 }));

            expectEquals(l.modEditCalls, 0);
            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // Front-panel program nav (cmd=0x0E)
        // -----------------------------------------------------------------
        beginTest("Front-panel nav +/- adjusts currentProgram and clamps at 0/99");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            engine.setCurrentProgram(54);
            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0E, 0x04 })); // UP
            expectEquals(engine.getCurrentProgram(), 55);
            expectEquals(l.progChangeCalls, 1);
            expectEquals(l.lastProgChange, 55);

            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0E, 0x08 })); // DOWN
            expectEquals(engine.getCurrentProgram(), 54);
            expectEquals(l.progChangeCalls, 2);

            engine.setCurrentProgram(99);
            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0E, 0x04 })); // UP at ceiling
            expectEquals(engine.getCurrentProgram(), 99, "clamps at 99");

            engine.setCurrentProgram(0);
            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x02, 0x0E, 0x08 })); // DOWN at floor
            expectEquals(engine.getCurrentProgram(), 0, "clamps at 0");

            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // Real MIDI Program Change
        // -----------------------------------------------------------------
        beginTest("A real MIDI Program Change updates currentProgram; out-of-range values are ignored");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            engine.processIncomingMessageForTest(nullptr, juce::MidiMessage::programChange(1, 42));
            expectEquals(engine.getCurrentProgram(), 42);
            expectEquals(l.progChangeCalls, 1);
            expectEquals(l.lastProgChange, 42);

            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // sysexID auto-learn
        // -----------------------------------------------------------------
        beginTest("sysexID auto-learns from an incoming device-ID byte that differs from current");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            expectEquals(engine.getSysexID(), 2, "default sysexID");

            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x05, 0x0B, 0x22, 0x00 }));
            expectEquals(engine.getSysexID(), 5);
            expectEquals(l.sysexIdCalls, 1);
            expectEquals(l.lastSysexId, 5);

            // A second message with the SAME id must not re-notify.
            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x05, 0x0B, 0x22, 0x00 }));
            expectEquals(l.sysexIdCalls, 1, "no duplicate notification for an unchanged id");

            engine.removeListener(&l);
        }

        beginTest("sysexID auto-learn ignores an out-of-range device-ID byte (>=16)");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            engine.processIncomingMessageForTest(nullptr, sysex({ 0x10, 0x14, 0x0B, 0x22, 0x00 })); // 0x14 = 20
            expectEquals(engine.getSysexID(), 2, "unchanged -- 20 is out of the valid 0-15 range");
            expectEquals(l.sysexIdCalls, 0);

            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // CC automation interception
        // -----------------------------------------------------------------
        beginTest("A mapped CC (no synth-input source) is scaled and dispatched as a hardware parameter change");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            // id=0 in XpanderParams: page=PAGE_VCO1, paramCol=VCO_FREQ, min=0, max=63.
            engine.setCcMap(10, 0);

            auto cc = juce::MidiMessage::controllerEvent(1, 10, 64);
            engine.processIncomingMessageForTest(nullptr, cc);

            expectEquals(l.paramCalls, 1);
            expectEquals(l.lastPage, (int)Matrix12::PAGE_VCO1);
            expectEquals(l.lastParamCol, (int)Matrix12::VCO_FREQ);
            expectEquals(l.lastValue, (int)std::round(64.0 * 63.0 / 127.0));
            expect(!l.lastIsDelta);
            expect(!l.lastIsButton);

            engine.removeListener(&l);
        }

        beginTest("An unmapped CC (no synth-input source) does nothing");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            auto cc = juce::MidiMessage::controllerEvent(1, 77, 100); // never mapped
            engine.processIncomingMessageForTest(nullptr, cc);

            expectEquals(l.paramCalls, 0);
            engine.removeListener(&l);
        }

        // -----------------------------------------------------------------
        // Real-time messages are dropped before any listener notification
        // -----------------------------------------------------------------
        beginTest("Real-time messages (active sense, clock, start/stop/continue) are silently dropped");
        {
            MidiEngine engine;
            CaptureListener l;
            engine.addListener(&l);

            engine.processIncomingMessageForTest(nullptr, juce::MidiMessage(0xFE)); // active sense
            engine.processIncomingMessageForTest(nullptr, juce::MidiMessage::midiClock());
            engine.processIncomingMessageForTest(nullptr, juce::MidiMessage::midiStart());
            engine.processIncomingMessageForTest(nullptr, juce::MidiMessage::midiStop());
            engine.processIncomingMessageForTest(nullptr, juce::MidiMessage::midiContinue());

            expectEquals(l.paramCalls, 0);
            expectEquals(l.patchCalls, 0);
            expectEquals(l.modEditCalls, 0);
            expectEquals(l.progChangeCalls, 0);
            expect(l.allStatus.empty(), "real-time messages must not reach onStatusUpdate either");

            engine.removeListener(&l);
        }
    }
};

static MidiEngineCharacterizationTest midiEngineCharacterizationTestInstance;

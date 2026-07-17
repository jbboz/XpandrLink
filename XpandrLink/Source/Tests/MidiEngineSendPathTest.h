/*
  MidiEngineSendPathTest.h

  Code-quality plan Phase 3 (docs/plans/2026-07-16-code-quality-improvement-plan.md): send-path
  characterization via the new IMidiBackend seam. Picks up exactly where
  MidiEngineCharacterizationTest.h's header comment said Phase 2.5 stopped -- "outgoing byte-for-byte
  send-path capture... needs a live Timer/message-loop pump or an actual output seam". Drives the
  real, private send/queue-drain machinery (MidiEngine::timerCallback, via the
  drainSendQueueForTest() test seam) end-to-end through a MockMidiBackend, with no real MIDI device.

  Same "test the real code, not a mirror" motivation as Phase 2.5 (see
  [[feedback_characterization_tests]]): the burst-drag mod-amount test below exercises the actual
  integrated changeModulationAmount -> sendModAmountNow -> enqueueMessage -> timerCallback path,
  extending (not replacing) ModAmountCoalesceTest.h, which only unit-tests the pure predicate
  shouldCoalesceModAmount in isolation.

  Verifies: NFR-TIMING-01/02/03/05, NFR-HW-01, NFR-THREAD-03 (see docs/NFR.md).
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "MockMidiBackend.h"

class MidiEngineSendPathTest : public juce::UnitTest
{
public:
    MidiEngineSendPathTest() : juce::UnitTest("MidiEngine send path", "MidiEngine") {}

    // Repeatedly drains the send queue with short real sleeps in between, long enough to clear
    // every currently-defined settle delay (button-page 150ms is the longest). The queue-drain
    // timing is real wall-clock (juce::Time::getMillisecondCounter()), so there is no faster,
    // deterministic way to observe a multi-step queued sequence land on the mock without a live
    // Timer/MessageManager pump, which TestMain.cpp does not run.
    static void pumpSendQueue(MidiEngine& engine, int totalMs = 400)
    {
        for (int elapsed = 0; elapsed < totalMs; elapsed += 10)
        {
            engine.drainSendQueueForTest();
            juce::Thread::sleep(10);
        }
        engine.drainSendQueueForTest();
    }

    // cmd byte (getSysExData()[2]) for every message this suite cares about is 0x0F (mod-matrix)
    // or 0x0B (page-select); sub-command lives at index 6 for mod-matrix messages.
    static bool isModCmd(const juce::MidiMessage& m, int cmdByte)
    {
        if (!m.isSysEx() || m.getSysExDataSize() < 8) return false;
        auto* d = m.getSysExData();
        return d[2] == 0x0F && d[6] == cmdByte;
    }

    void runTest() override
    {
        beginTest("sendAllNotesOff does nothing when no output is open");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };   // never calls setMidiOutput -- backend stays closed

            engine.sendAllNotesOff();

            expect(mock.sentMessages.empty(), "must not send anything while the output is closed");
        }

        beginTest("sendAllNotesOff sends immediately (no queue) once the output is open");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };

            engine.setMidiOutput("Fake Synth");
            expect(mock.isOpen());
            expectEquals((int)mock.sentMessages.size(), 0);

            engine.sendAllNotesOff();

            expectEquals((int)mock.sentMessages.size(), 1);
            expect(mock.sentMessages[0].isAllNotesOff());
        }

        beginTest("sendTuneAll sends the MIDI Tune Request (0xF6) immediately");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };
            engine.setMidiOutput("Fake Synth");

            engine.sendTuneAll();

            expectEquals((int)mock.sentMessages.size(), 1);
            expectEquals((int)mock.sentMessages[0].getRawDataSize(), 1);
            expectEquals((int)mock.sentMessages[0].getRawData()[0], 0xF6);
        }

        beginTest("sendProgramChange sends immediately and updates currentProgram");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };
            engine.setMidiOutput("Fake Synth");

            engine.sendProgramChange(42);

            expectEquals((int)mock.sentMessages.size(), 1);
            expect(mock.sentMessages[0].isProgramChange());
            expectEquals(mock.sentMessages[0].getProgramChangeNumber(), 42);
            expectEquals(engine.getCurrentProgram(), 42);
        }

        beginTest("sendParameterToSynth (button) queues a page-select then a param SysEx, draining onto the mock");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };
            engine.setMidiOutput("Fake Synth");

            engine.sendParameterToSynth(0x22, 0x05, 42, /*isButton=*/true);
            pumpSendQueue(engine);

            expectEquals((int)mock.sentMessages.size(), 2, "expected a page-select then a param change");

            // Page-select: getSysExData() = [10 id 0B page mode] (F0/F7 stripped by sendSysex).
            auto& pageSel = mock.sentMessages[0];
            expect(pageSel.isSysEx());
            expectEquals((int)pageSel.getSysExDataSize(), 5);
            expectEquals((int)pageSel.getSysExData()[2], 0x0B);
            expectEquals((int)pageSel.getSysExData()[3], 0x22);
            expectEquals((int)pageSel.getSysExData()[4], 1, "button param forces subpage mode=1");

            // Param change: getSysExData() = [10 id 0A 00 col 00 00 00 val_lo val_hi].
            auto& param = mock.sentMessages[1];
            expect(param.isSysEx());
            expectEquals((int)param.getSysExDataSize(), 10);
            expectEquals((int)param.getSysExData()[2], 0x0A);
            expectEquals((int)param.getSysExData()[4], 0x05);
            expectEquals((int)param.getSysExData()[8], 42);
            expectEquals((int)param.getSysExData()[9], 0);
        }

        beginTest("sendPatchToSynth redirects to the scratchpad slot (99), dumps, and activates it");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };
            engine.setMidiOutput("Fake Synth");

            expect(engine.loadInitPatch(), "embedded init patch must load as a valid single patch");
            engine.sendPatchToSynth();
            pumpSendQueue(engine);

            expectEquals((int)mock.sentMessages.size(), 2, "expected a patch dump then an activating program change");

            auto& dump = mock.sentMessages[0];
            expect(dump.isSysEx());
            expectEquals((int)dump.getSysExData()[4], 99, "dump must be stamped to the scratchpad slot, not the patch's own slot");

            auto& activate = mock.sentMessages[1];
            expect(activate.isProgramChange());
            expectEquals(activate.getProgramChangeNumber(), 99);
        }

        beginTest("Burst-drag mod-amount coalescing: only the LAST value in a rapid burst is ever sent");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };
            engine.setMidiOutput("Fake Synth");

            const int destIndex = 8;   // VCA1 Vol (0x22, subpage 6) -- matches ModEditDecodeTest's example
            const int idSource  = 0;

            // Fire an entire burst back-to-back with NO sleep in between (matching a fast real
            // drag, where each call is microseconds apart): the first call sends immediately
            // (lastModAmountSendTime_ starts at 0, far outside any real counter's throttle
            // window -- matches ModAmountCoalesceTest.h's "First-ever call -> send now" case);
            // every subsequent call in this same burst lands well within the 50ms coalescing
            // window (kModCmdGapMs) of that first send and must only update the pending value,
            // not enqueue a new one. Deliberately NOT asserting anything about the mid-burst
            // "not sent yet" state here -- kModCmdGapMs is also the page-select settle delay, so
            // draining the first send's own queue takes about as long as the coalescing window
            // itself, making any precise mid-flight timing assertion inherently racy. The robust,
            // meaningful invariant is checked once everything has fully settled below: total
            // sends stay far below the number of burst calls, and the last one wins.
            engine.changeModulationAmount(destIndex, idSource, 10);   // sends immediately
            engine.changeModulationAmount(destIndex, idSource, 20);   // coalesced
            engine.changeModulationAmount(destIndex, idSource, 30);   // coalesced
            engine.changeModulationAmount(destIndex, idSource, 40);   // coalesced -- last value in the burst

            // Let the throttle window fully pass (real sleep) and drain everything.
            juce::Thread::sleep(70);
            pumpSendQueue(engine);

            int setAmtCount = 0;
            int lastAmt = -1;
            for (auto& m : mock.sentMessages)
                if (isModCmd(m, 0x03)) { ++setAmtCount; lastAmt = (int)m.getSysExData()[8]; }
            expectEquals(setAmtCount, 2,
                "exactly one immediate send plus one coalesced flush -- never one send per burst call");
            expectEquals(lastAmt, 40, "the flushed value must be the LAST one from the burst, not an intermediate");
        }
    }
};

static MidiEngineSendPathTest midiEngineSendPathTestInstance;

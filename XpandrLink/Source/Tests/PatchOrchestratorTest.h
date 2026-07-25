/*
  PatchOrchestratorTest.h
  Unit tests for PatchOrchestrator (Phase 4 continued / TASK-13b step 3): the patch-assembly
  and one-level-undo logic extracted from EditorTabComponent. PatchOrchestrator has zero
  juce::Component dependencies -- UI state is read through injected callbacks -- so these
  tests exercise the real class directly, no UI needed.
*/
#pragma once
#include <JuceHeader.h>
#include "../Tabs/PatchOrchestrator.h"
#include "../Engine/MidiEngine.h"
#include "../Data/OberheimDefs.h"
#include "MockMidiBackend.h"

class PatchOrchestratorTest : public juce::UnitTest
{
public:
    PatchOrchestratorTest() : juce::UnitTest("PatchOrchestrator", "Tabs") {}

    void runTest() override
    {
        beginTest("encodePatchSysex packs header, mod bytes, and name at the documented offsets");
        {
            MidiEngine engine;
            PatchOrchestrator orch(engine, [] { return std::map<int,int>{}; },
                                          [] { return std::array<uint8_t,60>{}; },
                                          [] { return juce::String("--------"); });

            std::array<uint8_t,60> modBytes{};
            modBytes.fill((uint8_t)0xAB);
            auto sysex = orch.encodePatchSysex({}, modBytes.data(), "TESTNAME");

            expectEquals((int)sysex.size(), 399, "single-patch dump is always 399 bytes");
            expectEquals((int)sysex[0], 0xF0, "SysEx start");
            expectEquals((int)sysex[1], 0x10, "Oberheim manufacturer ID");
            expectEquals((int)sysex[2], engine.getSysexID(), "device ID byte");
            expectEquals((int)sysex[3], 0x01, "single-patch dump command");
            expectEquals((int)sysex[398], 0xF7, "SysEx end");

            // Mod region: decoded bytes 128-187 packed at sysex offset 6+2*128=262.
            expectEquals((int)sysex[262], 0x2B, "mod byte lo (0xAB & 0x7F)");
            expectEquals((int)sysex[263], 1,    "mod byte hi bit (0xAB bit 7)");

            // Name region: decoded bytes 188-195 packed at sysex offset 6+2*188=382.
            expectEquals((int)sysex[382], (int)'T', "first name char, lo byte");
            expectEquals((int)sysex[383], 0,         "ASCII never sets the hi bit");
        }

        beginTest("encodePatchSysex with nullptr modBytes leaves the mod region zero");
        {
            MidiEngine engine;
            PatchOrchestrator orch(engine, [] { return std::map<int,int>{}; },
                                          [] { return std::array<uint8_t,60>{}; },
                                          [] { return juce::String("--------"); });
            auto sysex = orch.encodePatchSysex({}, nullptr, "--------");
            expectEquals((int)sysex[262], 0, "mod region stays zero with no modBytes supplied");
        }

        beginTest("buildCurrentPatchSysex encodes via the injected callbacks");
        {
            MidiEngine engine;
            std::array<uint8_t,60> mod{};
            mod[0] = 0x42;
            PatchOrchestrator orch(engine,
                [] { return std::map<int,int>{ {1, 50} }; },
                [mod] { return mod; },
                [] { return juce::String("CURRENT "); });

            auto viaBuild    = orch.buildCurrentPatchSysex();
            auto viaExplicit = orch.encodePatchSysex({ {1, 50} }, mod.data(), "CURRENT ");
            expect(viaBuild == viaExplicit, "buildCurrentPatchSysex must match an equivalent explicit encode");
        }

        beginTest("revert restores the pre-roll snapshot and clears canRevert");
        {
            MidiEngine engine;
            PatchOrchestrator orch(engine, [] { return std::map<int,int>{}; },
                                          [] { return std::array<uint8_t,60>{}; },
                                          [] { return juce::String("BEFORE  "); });

            expect(!orch.canRevert(), "no snapshot taken yet");
            auto beforeSnapshot = orch.buildCurrentPatchSysex();
            orch.snapshotForRevert();
            expect(orch.canRevert(), "snapshot taken -- revert should now be available");

            // Simulate a roll landing a different patch in the cache.
            auto rolled = orch.encodePatchSysex({ {1, 99} }, nullptr, "AFTER   ");
            engine.setCachedPatch(rolled);

            orch.revert();
            expect(!orch.canRevert(), "revert clears the snapshot -- no double-revert");
            expect(engine.getCachedPatch() == beforeSnapshot, "revert must restore exactly the pre-roll bytes");
        }

        beginTest("applyPatch caches the newly encoded patch");
        {
            MidiEngine engine;
            PatchOrchestrator orch(engine, [] { return std::map<int,int>{}; },
                                          [] { return std::array<uint8_t,60>{}; },
                                          [] { return juce::String("ROLLED  "); });
            std::array<uint8_t,60> mod{};
            mod[5] = 7;
            orch.applyPatch({ {2, 10} }, mod);

            auto cached   = engine.getCachedPatch();
            auto expected = orch.encodePatchSysex({ {2, 10} }, mod.data(), "ROLLED  ");
            expect(cached == expected, "applyPatch must cache the patch it just encoded");
        }

        beginTest("applyPatch sends All-Notes-Off before the patch dump reaches the wire, targeting the scratchpad slot");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };
            engine.setMidiOutput("Fake Synth");

            PatchOrchestrator orch(engine, [] { return std::map<int,int>{}; },
                                          [] { return std::array<uint8_t,60>{}; },
                                          [] { return juce::String("ROLLED  "); });

            std::array<uint8_t,60> mod{};
            orch.applyPatch({ {2, 10} }, mod);

            // sendAllNotesOff() sends synchronously (no queue); sendPatchToSynth() enqueues.
            // Immediately after applyPatch() returns, only the All-Notes-Off should be on the wire.
            expectEquals((int)mock.sentMessages.size(), 1, "All-Notes-Off sends immediately; the patch dump is still queued");
            expect(mock.sentMessages[0].isAllNotesOff(), "the one immediate message must be All-Notes-Off (TASK-07 stuck-note fix)");

            // Drain the queue to observe the patch dump land afterward.
            for (int elapsed = 0; elapsed < 400; elapsed += 10)
            {
                engine.drainSendQueueForTest();
                juce::Thread::sleep(10);
            }
            engine.drainSendQueueForTest();

            expect(mock.sentMessages.size() >= 2, "the queued patch dump must eventually reach the wire too");
            auto& dump = mock.sentMessages[1];
            expect(dump.isSysEx(), "the queued message is the patch dump SysEx");
            // getSysExData() is F0-stripped by MidiEngine::sendSysex/createSysExMessage, so the
            // program-number byte at full-sysex offset 5 lands at getSysExData()[4] (see
            // MidiEngineSendPathTest.h's "redirects to the scratchpad slot" case for the same offset).
            expectEquals((int)dump.getSysExData()[4], Oberheim::kScratchpadProgram,
                        "BUG-32: the dump must target the scratchpad slot, never the patch's own stored slot");
        }

        beginTest("revert() restores the snapshot through the same send path (queued dump to the scratchpad slot)");
        {
            auto mockPtr = std::make_unique<MockMidiBackend>();
            auto& mock = *mockPtr;
            MidiEngine engine{ std::move(mockPtr) };
            engine.setMidiOutput("Fake Synth");

            PatchOrchestrator orch(engine, [] { return std::map<int,int>{}; },
                                          [] { return std::array<uint8_t,60>{}; },
                                          [] { return juce::String("BEFORE  "); });

            orch.snapshotForRevert();
            orch.revert();

            for (int elapsed = 0; elapsed < 400; elapsed += 10)
            {
                engine.drainSendQueueForTest();
                juce::Thread::sleep(10);
            }
            engine.drainSendQueueForTest();

            expect(!mock.sentMessages.empty(), "revert must actually send the restored patch");
            auto& dump = mock.sentMessages[0];
            expect(dump.isSysEx(), "revert sends the restored patch as a SysEx dump");
            expectEquals((int)dump.getSysExData()[4], Oberheim::kScratchpadProgram,
                        "revert also routes through the scratchpad slot, same as any other load");
        }
    }
};

static PatchOrchestratorTest patchOrchestratorTestInstance;

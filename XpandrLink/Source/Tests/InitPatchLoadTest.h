/*
  InitPatchLoadTest.h
  Unit tests for the embedded-init-patch load path (loadInitPatch /
  loadPatchFromMemory). Verifies the compiled-in BinaryData::InitPatch_syx
  parses as a valid 399-byte single patch and broadcasts a 196-byte decoded
  patch to listeners, and that an invalid buffer is rejected.
*/
#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"
#include "../Engine/MidiEngine.h"

class InitPatchLoadTest : public juce::UnitTest
{
public:
    InitPatchLoadTest() : juce::UnitTest("Init Patch Load", "MidiEngine") {}

    // Minimal listener that captures the last onPatchReceived payload.
    struct CaptureListener : public MidiEngine::Listener
    {
        int    calls = 0;
        int    lastProg = -1;
        size_t lastRawSize = 0;
        void onMidiActivity(bool) override {}
        void onPatchReceived(const std::vector<uint8_t>& raw, int prog) override
        {
            ++calls; lastProg = prog; lastRawSize = raw.size();
        }
        void onParameterChangedFromHardware(int, int, int, bool, bool) override {}
        void onStatusUpdate(const juce::String&) override {}
    };

    void runTest() override
    {
        beginTest("Embedded init patch is a valid 399-byte single patch");
        {
            expect(BinaryData::InitPatch_syxSize == 399);
            auto* b = reinterpret_cast<const uint8_t*>(BinaryData::InitPatch_syx);
            expect(b[0] == 0xF0);
            expect(b[398] == 0xF7);
            expect(b[1] == 0x10);   // mfr
            expect(b[3] == 0x01);   // cmd
        }

        beginTest("loadInitPatch broadcasts a 196-byte decoded patch");
        {
            MidiEngine engine;
            CaptureListener listener;
            engine.addListener(&listener);

            bool ok = engine.loadInitPatch();

            expect(ok);
            expectEquals(listener.calls, 1);
            expectEquals((int)listener.lastRawSize, 196);

            engine.removeListener(&listener);
        }

        beginTest("loadPatchFromMemory rejects a too-short buffer");
        {
            MidiEngine engine;
            CaptureListener listener;
            engine.addListener(&listener);

            const uint8_t junk[] = { 0xF0, 0x10, 0x00, 0xF7 };
            bool ok = engine.loadPatchFromMemory(junk, sizeof(junk));

            expect(! ok);
            expectEquals(listener.calls, 0);

            engine.removeListener(&listener);
        }
    }
};

static InitPatchLoadTest initPatchLoadTestInstance;

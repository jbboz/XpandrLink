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

        beginTest("Embedded init patch's empty mod-matrix slots use the hardware NONE sentinel, not zero");
        {
            // Root cause: the Xpander/Matrix-12 hardware only treats a
            // mod-matrix slot as *unused* when source==0x1F and dest==0x3F -- confirmed
            // against the C# reference's own serializer (XpanderTone.cs ~845-849, which
            // explicitly substitutes UNUSED_ENTRY_SOURCE_VALUE/UNUSED_ENTRY_DEST_VALUE for
            // any slot whose source is NONE before writing the patch out). A slot with
            // source=0/dest=0/amount=0 is NOT recognized as empty by the hardware -- it
            // reads as a real (silent) VCO1_FRQ<-ENV1 routing and still consumes one of
            // the synth's 20 total mod-matrix slots. The embedded InitPatch.syx previously
            // zero-filled its 18 unused slots instead of using the sentinel, so loading
            // Init Patch left the hardware believing all 20 slots were occupied -- any
            // further add (from the front panel or the editor) was rejected with
            // "maximum of 20 modulations per voice" even though only 2 routings showed
            // in the UI (our own decode already treats amount==0 as inactive for display,
            // which is why the UI never showed the problem).
            auto* b = reinterpret_cast<const uint8_t*>(BinaryData::InitPatch_syx);
            const uint8_t* packed = b + 6;   // header: F0 10 id 01 00 progNum

            auto unpack = [](uint8_t lo, uint8_t hi) -> uint8_t {
                return (uint8_t)(((hi & 0x01) << 7) | (lo & 0x7F));
            };

            for (int slot = 0; slot < 20; ++slot)
            {
                int base = 128 + slot * 3;
                uint8_t src = unpack(packed[2 * base],       packed[2 * base + 1]);
                uint8_t amt = unpack(packed[2 * (base + 1)], packed[2 * (base + 1) + 1]);
                uint8_t dst = unpack(packed[2 * (base + 2)], packed[2 * (base + 2) + 1]);

                bool amountIsZero = (amt & 0x3F) == 0;
                if (amountIsZero)
                {
                    expectEquals((int)src, 0x1F, "slot " + juce::String(slot) + " should use the NONE source sentinel");
                    expectEquals((int)dst, 0x3F, "slot " + juce::String(slot) + " should use the NONE dest sentinel");
                }
            }
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

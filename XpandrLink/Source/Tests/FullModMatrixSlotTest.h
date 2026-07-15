/*
  FullModMatrixSlotTest.h
  Slot-keyed API tests for FullModMatrixPanel , mirroring
  IDSourceTrackingTest.h's ModSummaryPanel coverage. Hardware front-panel mod
  edits (cmd=0x0F) address an entry by (destIdx, idSource) alone -- the existing
  srcIdx+destIdx-keyed methods (updateEntrySource, removeEntry, etc.) can't look
  that up on their own.
*/
#pragma once
#include <JuceHeader.h>
#include "../UI/FullModMatrixPanel.h"

class FullModMatrixSlotTest : public juce::UnitTest
{
public:
    FullModMatrixSlotTest() : juce::UnitTest("FullModMatrixPanel slot API", "ModMatrix") {}

    void runTest() override
    {
        beginTest("getEntryAtSlot finds the entry by (destIdx, idSource) alone");
        {
            FullModMatrixPanel panel;
            panel.addEntry(19, 6, 32, 0);   // LFO1 -> dest6, slot 0
            panel.addEntry(20, 6, 40, 1);   // LFO2 -> dest6, slot 1

            FullModMatrixPanel::SlotEntry out;
            expect(panel.getEntryAtSlot(6, 0, out), "expected slot 0 to be found");
            expectEquals(out.srcIdx, 19);
            expectEquals(out.amount, 32);
            expect(panel.getEntryAtSlot(6, 1, out), "expected slot 1 to be found");
            expectEquals(out.srcIdx, 20);
        }

        beginTest("getEntryAtSlot returns false for an empty slot");
        {
            FullModMatrixPanel panel;
            panel.addEntry(19, 6, 32, 0);
            FullModMatrixPanel::SlotEntry out;
            expect(!panel.getEntryAtSlot(6, 3, out), "slot 3 should be empty");
            expect(!panel.getEntryAtSlot(9, 0, out), "dest 9 has no entries");
        }

        beginTest("setSourceAtSlot changes the source in place, keeps the slot");
        {
            FullModMatrixPanel panel;
            panel.addEntry(19, 6, 32, 0);
            panel.setSourceAtSlot(6, 0, 20);
            FullModMatrixPanel::SlotEntry out;
            expect(panel.getEntryAtSlot(6, 0, out), "slot 0 should still exist");
            expectEquals(out.srcIdx, 20);
            expectEquals(panel.getIdSourceForEntry(20, 6), 0);
        }

        beginTest("setAmountAtSlot changes the amount, e.g. DIALVALUEAMOUNTOFCHANGE +1");
        {
            FullModMatrixPanel panel;
            panel.addEntry(19, 6, 10, 0);
            panel.setAmountAtSlot(6, 0, 11);
            FullModMatrixPanel::SlotEntry out;
            expect(panel.getEntryAtSlot(6, 0, out), "slot 0 should still exist");
            expectEquals(out.amount, 11);
        }

        beginTest("setQuantizeAtSlot toggles quantize on the right slot");
        {
            FullModMatrixPanel panel;
            panel.addEntry(19, 6, 10, 0);
            FullModMatrixPanel::SlotEntry before;
            panel.getEntryAtSlot(6, 0, before);
            expect(!before.quantize, "quantize should start false");
            panel.setQuantizeAtSlot(6, 0, true);
            FullModMatrixPanel::SlotEntry after;
            panel.getEntryAtSlot(6, 0, after);
            expect(after.quantize, "expected quantize to be set true");
        }

        beginTest("removeAtSlot removes only the targeted slot");
        {
            FullModMatrixPanel panel;
            panel.addEntry(19, 6, 32, 0);
            panel.addEntry(20, 6, 40, 1);
            panel.removeAtSlot(6, 0);
            FullModMatrixPanel::SlotEntry out;
            expect(!panel.getEntryAtSlot(6, 0, out), "slot 0 should be gone");
            expect(panel.getEntryAtSlot(6, 1, out), "slot 1 should be untouched");
            expectEquals(out.srcIdx, 20);
        }

        beginTest("Removing an entry compacts the grid -- later entries shift down, no gap");
        {
            // Three entries at three different destinations land in grid slots 0, 1, 2
            // (addEntry always fills the first free slot). Removing the middle one
            // (slot 1) should shift the third entry down into slot 1, not leave a hole.
            FullModMatrixPanel panel;
            panel.addEntry(1, 6,  10, 0);    // -> grid slot 0
            panel.addEntry(2, 9,  20, 0);    // -> grid slot 1
            panel.addEntry(3, 15, 30, 0);    // -> grid slot 2

            panel.removeEntry(2, 9);         // remove the middle entry

            // Hardware addressing (destIdx, idSource) is untouched by the shift --
            // dest 6 and dest 15 each still have exactly one entry at idSource 0.
            FullModMatrixPanel::SlotEntry out;
            expect(panel.getEntryAtSlot(6, 0, out), "dest 6 entry should still be findable");
            expectEquals(out.srcIdx, 1);
            expect(panel.getEntryAtSlot(15, 0, out), "dest 15 entry should still be findable");
            expectEquals(out.srcIdx, 3);

            // The grid/wire-order position is what actually compacted: dest 15's entry
            // (originally wire slot 2, bytes 6-8) should now be at wire slot 1 (bytes
            // 3-5), and slot 2 should be the unused sentinel, not a lingering gap.
            std::array<uint8_t, 60> mod{};
            panel.getCurrentModBytes(mod);
            expectEquals((int)mod[0], 1,  "slot 0 unchanged: src 1");
            expectEquals((int)mod[2], 6,  "slot 0 unchanged: dest 6");
            expectEquals((int)mod[3], 3,  "slot 1 now holds what was slot 2's source (3)");
            expectEquals((int)mod[5], 15, "slot 1 now holds what was slot 2's dest (15)");
            expectEquals((int)mod[6], 0x1F, "slot 2 is now the unused sentinel, not a gap");
            expectEquals((int)mod[8], 0x3F, "slot 2 dest sentinel");
        }

        beginTest("getCurrentModBytes sentinel-fills unused slots (0x1F/0x3F), never zero");
        {
            // Same convention already used by PatchRandomizer.h and required by the
            // hardware (see the Init Patch mod-matrix corruption fix): a zero-filled
            // unused slot decodes as a real ENV1->VCO1_FRQ routing, not "empty", and
            // can exhaust the synth's 20-slot budget with silent phantom entries.
            FullModMatrixPanel panel;
            panel.addEntry(19, 6, 32, 0);   // just one active entry; slots 1-19 unused

            std::array<uint8_t, 60> mod{};
            panel.getCurrentModBytes(mod);

            // Slot 0 (the active entry) should NOT be the sentinel.
            expectEquals((int)mod[0], 19, "slot 0 source should be the real active entry");

            for (int i = 1; i < 20; ++i)
            {
                int base = i * 3;
                expectEquals((int)mod[(size_t)base],     0x1F, "unused source sentinel, slot " + juce::String(i));
                expectEquals((int)mod[(size_t)base + 1], 0x00, "unused amount byte, slot " + juce::String(i));
                expectEquals((int)mod[(size_t)base + 2], 0x3F, "unused dest sentinel, slot " + juce::String(i));
            }
        }
    }
};

static FullModMatrixSlotTest fullModMatrixSlotTestInstance;

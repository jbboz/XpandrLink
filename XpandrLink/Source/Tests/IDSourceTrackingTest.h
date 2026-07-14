/*
  IDSourceTrackingTest.h
  Unit tests for Item 18: IDSource tracking in mod matrix panels.

  The hardware numbers modulation sources within a destination as 0, 1, 2...
  When we decode a patch, each active entry must receive the correct idSource
  (its ordinal among entries sharing the same destination).  A wrong idSource
  causes SETSIGN/SETUNSIGNEDVALUE SysEx to target the wrong slot.

  Tests cover:
    1. Decode assigns idSource in scan order per destination.
    2. Multiple destinations are tracked independently.
    3. getNextIdSourceForDest returns the first unused slot.
    4. decrementIdSourceAfterRemove shifts higher slots down by one.
    5. addEntry with explicit idSource stores it; without, stores -1.
*/
#pragma once
#include <JuceHeader.h>
#include "../UI/ModSummaryPanel.h"

class IDSourceTrackingTest : public juce::UnitTest
{
public:
    IDSourceTrackingTest() : juce::UnitTest("IDSource tracking", "ModMatrix") {}

    void runTest() override
    {
        // Build a minimal 196-byte decoded patch block (patchData indices 0..195).
        // Mod matrix occupies decoded bytes 128..187 (60 bytes = 20 entries × 3).
        // Entry layout: [source][amtByte][dest]
        //   amtByte: bits[5:0]=absAmt, bit6=sign, bit7=quantize
        // An entry is active if src<=26, dst<=46, amount!=0.

        // Helper lambda to set one entry in the patch vector.
        auto setEntry = [](std::vector<int>& data, int entryIdx, int src, int amt, int dst) {
            int base = 128 + entryIdx * 3;
            data[base]     = src;
            data[base + 1] = amt;
            data[base + 2] = dst;
        };

        // -----------------------------------------------------------------------
        // 1. Single source per destination: idSource = 0
        // -----------------------------------------------------------------------
        beginTest("Single source per dest -> idSource=0");
        {
            std::vector<int> data(196, 0);
            setEntry(data, 0, 19, 10, 6);   // LFO1 -> VCF_FREQ, amount=10
            setEntry(data, 1, 20, 5,  0);   // LFO2 -> VCO1_FRQ, amount=5

            ModSummaryPanel panel;
            panel.updateFromPatch(data);

            auto idA = panel.getIdSourceForEntry(19, 6);
            auto idB = panel.getIdSourceForEntry(20, 0);
            expectEquals(idA, 0);
            expectEquals(idB, 0);
        }

        // -----------------------------------------------------------------------
        // 2. Two sources -> same destination get idSource 0 and 1
        // -----------------------------------------------------------------------
        beginTest("Two sources, same dest -> idSource 0 and 1");
        {
            std::vector<int> data(196, 0);
            setEntry(data, 0, 19, 10, 6);   // LFO1 -> VCF_FREQ  -> idSource 0
            setEntry(data, 3, 20, 15, 6);   // LFO2 -> VCF_FREQ  -> idSource 1

            ModSummaryPanel panel;
            panel.updateFromPatch(data);

            expectEquals(panel.getIdSourceForEntry(19, 6), 0);
            expectEquals(panel.getIdSourceForEntry(20, 6), 1);
        }

        // -----------------------------------------------------------------------
        // 3. Three sources -> same destination; different dest independent
        // -----------------------------------------------------------------------
        beginTest("Three sources same dest + one other dest");
        {
            std::vector<int> data(196, 0);
            setEntry(data, 0, 19,  8, 6);   // LFO1 -> VCF_FREQ  -> idSource 0
            setEntry(data, 1, 20, 12, 6);   // LFO2 -> VCF_FREQ  -> idSource 1
            setEntry(data, 2, 21, 20, 6);   // LFO3 -> VCF_FREQ  -> idSource 2
            setEntry(data, 4, 12,  5, 0);   // ENV1 -> VCO1_FRQ  -> idSource 0 (own dest)

            ModSummaryPanel panel;
            panel.updateFromPatch(data);

            expectEquals(panel.getIdSourceForEntry(19, 6), 0);
            expectEquals(panel.getIdSourceForEntry(20, 6), 1);
            expectEquals(panel.getIdSourceForEntry(21, 6), 2);
            expectEquals(panel.getIdSourceForEntry(12, 0), 0);
        }

        // -----------------------------------------------------------------------
        // 4. getNextIdSourceForDest returns first unused slot
        // -----------------------------------------------------------------------
        beginTest("getNextIdSourceForDest - first free slot");
        {
            std::vector<int> data(196, 0);
            setEntry(data, 0, 19, 10, 6);   // slot 0 used
            setEntry(data, 1, 20, 15, 6);   // slot 1 used

            ModSummaryPanel panel;
            panel.updateFromPatch(data);

            expectEquals(panel.getNextIdSourceForDest(6), 2);   // next free = 2
            expectEquals(panel.getNextIdSourceForDest(0), 0);   // dest 0 unused -> free = 0
        }

        // -----------------------------------------------------------------------
        // 5. decrementIdSourceAfterRemove shifts higher slots
        // -----------------------------------------------------------------------
        beginTest("decrementIdSourceAfterRemove shifts slots above removed");
        {
            std::vector<int> data(196, 0);
            setEntry(data, 0, 19, 10, 6);   // LFO1 -> dest6, idSource 0
            setEntry(data, 1, 20, 15, 6);   // LFO2 -> dest6, idSource 1
            setEntry(data, 2, 21, 20, 6);   // LFO3 -> dest6, idSource 2

            ModSummaryPanel panel;
            panel.updateFromPatch(data);

            // Simulate removing idSource=1 (LFO2)
            panel.removeEntry(20, 6);
            panel.decrementIdSourceAfterRemove(6, 1);

            // idSource 0 unchanged; former idSource 2 is now 1
            expectEquals(panel.getIdSourceForEntry(19, 6), 0);
            expectEquals(panel.getIdSourceForEntry(21, 6), 1);
            // Next free slot should now be 2 again
            expectEquals(panel.getNextIdSourceForDest(6), 2);
        }

        // -----------------------------------------------------------------------
        // 6. addEntry with explicit idSource stores it
        // -----------------------------------------------------------------------
        beginTest("addEntry with explicit idSource");
        {
            ModSummaryPanel panel;
            panel.addEntry(19, 6, 32, 3);
            expectEquals(panel.getIdSourceForEntry(19, 6), 3);
        }

        // -----------------------------------------------------------------------
        // 7. addEntry without idSource stores -1
        // -----------------------------------------------------------------------
        beginTest("addEntry without idSource stores -1");
        {
            ModSummaryPanel panel;
            panel.addEntry(19, 6, 32);
            expectEquals(panel.getIdSourceForEntry(19, 6), -1);
        }

        // -----------------------------------------------------------------------
        // Slot-keyed API: hardware front-panel mod edits address an
        // entry by (destIdx, idSource) alone -- e.g. DIALVALUEAMOUNTOFCHANGE only
        // gives a slot number and destination, not which source occupies that slot.
        // The existing srcIdx+destIdx-keyed methods above can't look that up.
        // -----------------------------------------------------------------------

        beginTest("getEntryAtSlot finds the entry by (destIdx, idSource) alone");
        {
            ModSummaryPanel panel;
            panel.addEntry(19, 6, 32, 0);   // LFO1 -> dest6, slot 0
            panel.addEntry(20, 6, 40, 1);   // LFO2 -> dest6, slot 1

            ModSummaryPanel::SlotEntry out;
            expect(panel.getEntryAtSlot(6, 0, out), "expected slot 0 to be found");
            expectEquals(out.srcIdx, 19);
            expectEquals(out.amount, 32);
            expect(panel.getEntryAtSlot(6, 1, out), "expected slot 1 to be found");
            expectEquals(out.srcIdx, 20);
        }

        beginTest("getEntryAtSlot returns false for an empty slot");
        {
            ModSummaryPanel panel;
            panel.addEntry(19, 6, 32, 0);
            ModSummaryPanel::SlotEntry out;
            expect(!panel.getEntryAtSlot(6, 3, out), "slot 3 should be empty");
            expect(!panel.getEntryAtSlot(9, 0, out), "dest 9 has no entries");
        }

        beginTest("setSourceAtSlot changes the source in place, keeps the slot");
        {
            ModSummaryPanel panel;
            panel.addEntry(19, 6, 32, 0);
            panel.setSourceAtSlot(6, 0, 20);   // matches a hardware CHANGESOURCE
            ModSummaryPanel::SlotEntry out;
            expect(panel.getEntryAtSlot(6, 0, out), "slot 0 should still exist");
            expectEquals(out.srcIdx, 20);
            expectEquals(panel.getIdSourceForEntry(20, 6), 0);
        }

        beginTest("setAmountAtSlot changes the amount, e.g. DIALVALUEAMOUNTOFCHANGE +1");
        {
            ModSummaryPanel panel;
            panel.addEntry(19, 6, 10, 0);
            panel.setAmountAtSlot(6, 0, 11);
            ModSummaryPanel::SlotEntry out;
            expect(panel.getEntryAtSlot(6, 0, out), "slot 0 should still exist");
            expectEquals(out.amount, 11);
        }

        beginTest("setQuantizeAtSlot toggles quantize on the right slot");
        {
            ModSummaryPanel panel;
            panel.addEntry(19, 6, 10, 0);
            ModSummaryPanel::SlotEntry before;
            panel.getEntryAtSlot(6, 0, before);
            expect(!before.quantize, "quantize should start false");
            panel.setQuantizeAtSlot(6, 0, true);
            ModSummaryPanel::SlotEntry after;
            panel.getEntryAtSlot(6, 0, after);
            expect(after.quantize, "expected quantize to be set true");
        }

        beginTest("removeAtSlot removes only the targeted slot");
        {
            ModSummaryPanel panel;
            panel.addEntry(19, 6, 32, 0);
            panel.addEntry(20, 6, 40, 1);
            panel.removeAtSlot(6, 0);
            ModSummaryPanel::SlotEntry out;
            expect(!panel.getEntryAtSlot(6, 0, out), "slot 0 should be gone");
            expect(panel.getEntryAtSlot(6, 1, out), "slot 1 should be untouched");
            expectEquals(out.srcIdx, 20);
        }
    }
};

static IDSourceTrackingTest idSourceTrackingTestInstance;

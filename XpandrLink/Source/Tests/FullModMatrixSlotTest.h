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
    }
};

static FullModMatrixSlotTest fullModMatrixSlotTestInstance;

/*
  ModEditDecodeTest.h
  Unit tests for decoding hardware front-panel modulation-matrix edits (cmd=0x0F).

  Root cause (session 60): a "Get Patch" dump request does NOT reflect front-panel
  mod-matrix edits on real hardware -- confirmed via live capture showing the same
  stale mod-matrix bytes returned after repeated dump requests despite a live
  routing add. The correct approach (matching the C# reference,
  XpanderController.MIDIEvents.cs HandleModulationEditFromSynth) is to decode the
  ModulationEdit SysEx directly instead of re-requesting a dump.

  Message layout (JUCE data[], F0/F7 excluded):
    data[0]=0x10 data[1]=id data[2]=0x0F data[3]=0x00
    data[4]=idSource(slot 0-5) data[5]=0x00 data[6]=command data[7]=0x00
    data[8]=valueLo data[9]=valueHi

  Destination is not in the message -- it's inferred from the current page/subpage
  (tracked via cmd=0x0B PageSelect) through kModDestTable, exposed here via
  MidiEngine::destIndexForPageSubPage.
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"

class ModEditDecodeTest : public juce::UnitTest
{
public:
    ModEditDecodeTest() : juce::UnitTest("ModulationEdit decode", "MidiEngine") {}

    void runTest() override
    {
        beginTest("destIndexForPageSubPage resolves known page/subpage pairs");
        {
            expectEquals(MidiEngine::destIndexForPageSubPage(0x20, 2), 0);   // VCO1 FRQ
            expectEquals(MidiEngine::destIndexForPageSubPage(0x22, 6), 8);   // VCA1 Vol
            expectEquals(MidiEngine::destIndexForPageSubPage(0x22, 7), 9);   // VCA2 Vol
            expectEquals(MidiEngine::destIndexForPageSubPage(0x2C, 7), 44);  // Env5 Amp
            expectEquals(MidiEngine::destIndexForPageSubPage(0x23, 6), 46);  // Lag Rate
        }

        beginTest("destIndexForPageSubPage returns -1 for a non-destination page/subpage");
        {
            expectEquals(MidiEngine::destIndexForPageSubPage(0x22, 0), -1);
            expectEquals(MidiEngine::destIndexForPageSubPage(0x99, 2), -1);
        }

        beginTest("decodeModulationEditMessage: live-captured ADDSOURCE (LFO1 -> VCA1 Vol)");
        {
            // RX: SysEx [10] 10 2 f 0 0 0 0 0 13 0  at page=0x22 mode=6
            std::vector<uint8_t> data = { 0x10, 0x02, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00 };
            MidiEngine::ModEdit edit;
            bool ok = MidiEngine::decodeModulationEditMessage(data.data(), (int)data.size(), 0x22, 6, edit);
            expect(ok, "expected a valid decode");
            expectEquals(edit.destIndex, 8);
            expectEquals(edit.idSource, 0);
            expect(edit.command == MidiEngine::ModEditCommand::AddSource, "expected AddSource");
            expectEquals(edit.value, 19);   // LFO1 source id
        }

        beginTest("decodeModulationEditMessage: live-captured CHANGESOURCE confirm-echo");
        {
            // RX: SysEx [10] 10 2 f 0 0 0 2 0 13 0  at page=0x22 mode=6
            std::vector<uint8_t> data = { 0x10, 0x02, 0x0F, 0x00, 0x00, 0x00, 0x02, 0x00, 0x13, 0x00 };
            MidiEngine::ModEdit edit;
            bool ok = MidiEngine::decodeModulationEditMessage(data.data(), (int)data.size(), 0x22, 6, edit);
            expect(ok, "expected a valid decode");
            expectEquals(edit.destIndex, 8);
            expectEquals(edit.idSource, 0);
            expect(edit.command == MidiEngine::ModEditCommand::ChangeSource, "expected ChangeSource");
            expectEquals(edit.value, 19);
        }

        beginTest("decodeModulationEditMessage: live-captured DIALVALUEAMOUNTOFCHANGE (+1)");
        {
            // RX: SysEx [10] 10 2 f 0 0 0 4 0 1 0  at page=0x22 mode=6
            std::vector<uint8_t> data = { 0x10, 0x02, 0x0F, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00 };
            MidiEngine::ModEdit edit;
            bool ok = MidiEngine::decodeModulationEditMessage(data.data(), (int)data.size(), 0x22, 6, edit);
            expect(ok, "expected a valid decode");
            expect(edit.command == MidiEngine::ModEditCommand::DialValueAmountOfChange, "expected DialValueAmountOfChange");
            expectEquals(edit.value, 1);
        }

        beginTest("decodeModulationEditMessage: negative (sign bit set) value decodes correctly");
        {
            // Same DIAL command but valueHi=1 (sign bit) with valueLo=0x7E -> -(0x80-0x7E) = -2
            std::vector<uint8_t> data = { 0x10, 0x02, 0x0F, 0x00, 0x00, 0x00, 0x04, 0x00, 0x7E, 0x01 };
            MidiEngine::ModEdit edit;
            bool ok = MidiEngine::decodeModulationEditMessage(data.data(), (int)data.size(), 0x22, 6, edit);
            expect(ok, "expected a valid decode");
            expectEquals(edit.value, -2);
        }

        beginTest("decodeModulationEditMessage: fails on a too-short buffer");
        {
            std::vector<uint8_t> data = { 0x10, 0x02, 0x0F, 0x00, 0x00 };
            MidiEngine::ModEdit edit;
            bool ok = MidiEngine::decodeModulationEditMessage(data.data(), (int)data.size(), 0x22, 6, edit);
            expect(!ok, "expected decode to fail on short buffer");
        }

        beginTest("decodeModulationEditMessage: fails when the current page/subpage isn't a mod destination");
        {
            std::vector<uint8_t> data = { 0x10, 0x02, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00 };
            MidiEngine::ModEdit edit;
            bool ok = MidiEngine::decodeModulationEditMessage(data.data(), (int)data.size(), 0x22, 0, edit);
            expect(!ok, "expected decode to fail when destination can't be resolved");
        }
    }
};

static ModEditDecodeTest modEditDecodeTestInstance;

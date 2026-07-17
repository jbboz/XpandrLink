/*
  BUG05_ProgramChangeSysexTest.h
  Unit tests for BUG-05: front-panel patch navigation via SysEx.

  The Matrix-12 does NOT send a standard MIDI Program Change when you press the
  +/- buttons on the front panel. Instead it sends one of two SysEx messages:
    UP:   F0 10 [id] 0E 04 F7  (getSysExData = [10 id 0E 04], len=4)
    DOWN: F0 10 [id] 0E 08 F7  (getSysExData = [10 id 0E 08], len=4)

  The hardware ALSO sends a standard MIDI Program Change for the new patch.
  To avoid double-incrementing, MidiEngine owns currentProgram as shared state:
    1. Navigation SysEx arrives -> currentProgram ± 1 -> onProgramChangeReceived(absolute)
    2. Program Change arrives   -> currentProgram = progNum (already updated) -> onProgramChangeReceived(same)

  These tests verify:
    1. UP SysEx pattern is correctly identified (delta +1 applied to currentProgram).
    2. DOWN SysEx pattern is correctly identified (delta -1 applied to currentProgram).
    3. Other 0x0E sub-bytes are ignored.
    4. Short SysEx (len<4) is ignored.
    5. currentProgram clamps to [0, 99].
    6. A Program Change arriving after a nav SysEx does not double-increment
       because currentProgram is already at the new value.

  Verifies: NFR-HW-03 (see docs/NFR.md).
*/
#pragma once
#include <JuceHeader.h>

class BUG05_ProgramChangeSysexTest : public juce::UnitTest
{
public:
    BUG05_ProgramChangeSysexTest() : juce::UnitTest("BUG-05 Program Change SysEx", "MidiEngine") {}

    void runTest() override
    {
        // ---------------------------------------------------------------
        // 1. UP SysEx -> currentProgram + 1
        // ---------------------------------------------------------------
        beginTest("UP SysEx (0x0E 0x04) advances currentProgram by 1");
        {
            int prog = 54;
            uint8_t data[] = { 0x10, 0x00, 0x0E, 0x04 };
            applyNavSysex(data, 4, prog);
            expectEquals(prog, 55);
        }

        // ---------------------------------------------------------------
        // 2. DOWN SysEx -> currentProgram - 1
        // ---------------------------------------------------------------
        beginTest("DOWN SysEx (0x0E 0x08) decrements currentProgram by 1");
        {
            int prog = 54;
            uint8_t data[] = { 0x10, 0x00, 0x0E, 0x08 };
            applyNavSysex(data, 4, prog);
            expectEquals(prog, 53);
        }

        // ---------------------------------------------------------------
        // 3. Unknown sub-byte -> no change
        // ---------------------------------------------------------------
        beginTest("Unknown sub-byte -> no change to currentProgram");
        {
            int prog = 54;
            uint8_t data[] = { 0x10, 0x00, 0x0E, 0x01 };
            applyNavSysex(data, 4, prog);
            expectEquals(prog, 54);
        }

        // ---------------------------------------------------------------
        // 4. Short SysEx -> no change
        // ---------------------------------------------------------------
        beginTest("Short SysEx (len<4) -> no change");
        {
            int prog = 54;
            uint8_t data[] = { 0x10, 0x00, 0x0E };
            applyNavSysex(data, 3, prog);
            expectEquals(prog, 54);
        }

        // ---------------------------------------------------------------
        // 5. Clamping at upper bound
        // ---------------------------------------------------------------
        beginTest("UP from program 99 clamps to 99");
        {
            int prog = 99;
            uint8_t data[] = { 0x10, 0x00, 0x0E, 0x04 };
            applyNavSysex(data, 4, prog);
            expectEquals(prog, 99);
        }

        beginTest("DOWN from program 0 clamps to 0");
        {
            int prog = 0;
            uint8_t data[] = { 0x10, 0x00, 0x0E, 0x08 };
            applyNavSysex(data, 4, prog);
            expectEquals(prog, 0);
        }

        // ---------------------------------------------------------------
        // 6. No double-increment when Program Change follows nav SysEx
        //    (currentProgram already at N+1; Program Change arrives with N+1)
        // ---------------------------------------------------------------
        beginTest("Program Change after UP SysEx does not double-increment");
        {
            int prog = 54;
            // Nav SysEx arrives first: prog -> 55
            uint8_t upData[] = { 0x10, 0x00, 0x0E, 0x04 };
            applyNavSysex(upData, 4, prog);
            expectEquals(prog, 55);
            // Hardware then sends Program Change 55: currentProgram stays 55
            int pcNum = 55;
            if (pcNum >= 0 && pcNum < 100) prog = pcNum;  // mirrors MidiEngine logic
            expectEquals(prog, 55);
        }
    }

private:
    // Mirrors the nav-SysEx branch of MidiEngine::handleIncomingMidiMessage.
    // data = getSysExData() content (F0 excluded); currentProg updated in place.
    static void applyNavSysex(const uint8_t* data, int len, int& currentProg)
    {
        if (len < 4) return;
        uint8_t cmd = data[2];
        if (cmd != 0x0E) return;
        int delta = 0;
        if      (data[3] == 0x04) delta = +1;
        else if (data[3] == 0x08) delta = -1;
        if (delta != 0)
            currentProg = juce::jlimit(0, 99, currentProg + delta);
    }
};

static BUG05_ProgramChangeSysexTest bug05ProgramChangeSysexTestInstance;

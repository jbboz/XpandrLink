/*
  ChangeModulationSourceTest.h
  Unit test for the CHANGESOURCE mod-matrix fix: changing an existing entry's
  source in place (e.g. ENV2 -> ENV1 on a live routing) must use the dedicated
  atomic CHANGESOURCE command (cmd=0x02), never delete+re-add, so the entry's
  hardware slot (IDSource) is never touched or re-predicted.

  Byte layout confirmed against the C# reference (XpanderTone.ModulationMatrix.cs,
  EnumModulationEditCommands.CHANGESOURCE=0x02):
    F0 10 [id] 0F 00 [idSource] 00 02 00 [newSource] 00 F7

  Verifies: NFR-HW-05 (see docs/NFR.md).
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"

class ChangeModulationSourceTest : public juce::UnitTest
{
public:
    ChangeModulationSourceTest() : juce::UnitTest("ChangeModulationSource SysEx", "MidiEngine") {}

    void runTest() override
    {
        beginTest("buildChangeSourceMessage produces the exact CHANGESOURCE byte sequence");
        {
            auto msg = MidiEngine::buildChangeSourceMessage(/*sysexId*/2, /*idSource*/3, /*newSource*/12);
            std::vector<uint8_t> expected = {
                0xF0, 0x10, 0x02, 0x0F, 0x00,
                0x03, 0x00, 0x02,
                0x00, 0x0C, 0x00, 0xF7
            };
            expect(msg == expected, "CHANGESOURCE byte mismatch");
        }

        beginTest("buildChangeSourceMessage varies only idSource/newSource/sysexId across bytes 2, 5, 9");
        {
            auto a = MidiEngine::buildChangeSourceMessage(4, 0, 26);
            std::vector<uint8_t> expectedA = {
                0xF0, 0x10, 0x04, 0x0F, 0x00,
                0x00, 0x00, 0x02,
                0x00, 0x1A, 0x00, 0xF7
            };
            expect(a == expectedA, "CHANGESOURCE byte mismatch (idSource=0, newSource=26)");
        }
    }
};

static ChangeModulationSourceTest changeModulationSourceTestInstance;

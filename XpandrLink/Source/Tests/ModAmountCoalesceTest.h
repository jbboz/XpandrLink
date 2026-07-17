/*
  ModAmountCoalesceTest.h
  Unit test for the mod-matrix amount-drag flooding fix (found 2026-07-13): dragging a
  mod-matrix amount knob fast (e.g. max to min on VCA1/VCA2 Vol) fires
  MidiEngine::changeModulationAmount dozens of times a second. Since SETSIGN+
  SETUNSIGNEDVALUE have zero delay between them, MidiEngine::timerCallback's
  drain-all-zero-delay-messages loop would send an unbounded burst with no pacing,
  flooding the synth's MIDI IN faster than its UART/firmware can keep up -- observed
  as a temporary hardware lockup that self-recovers once the flood stops.

  MidiEngine::shouldCoalesceModAmount(now, lastSendTime, throttleMs) is the pure
  decision function: true means "still inside the throttle window, coalesce this
  update instead of sending it immediately."

  Verifies: NFR-TIMING-05 (see docs/NFR.md).
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"

class ModAmountCoalesceTest : public juce::UnitTest
{
public:
    ModAmountCoalesceTest() : juce::UnitTest("Mod Amount Coalesce", "MidiEngine") {}

    void runTest() override
    {
        beginTest("Within the throttle window -> coalesce (true)");
        {
            expect(MidiEngine::shouldCoalesceModAmount(1000, 980, 50), "elapsed=20 < 50 should coalesce");
        }

        beginTest("Exactly at the throttle boundary -> still coalesce (inclusive)");
        {
            expect(MidiEngine::shouldCoalesceModAmount(1030, 980, 50), "elapsed=50 == throttle should coalesce");
        }

        beginTest("Past the throttle window -> send now (false)");
        {
            expect(!MidiEngine::shouldCoalesceModAmount(1100, 980, 50), "elapsed=120 >= 50 should send now");
        }

        beginTest("First-ever call (lastSendTime=0, now is any real counter value) -> send now");
        {
            expect(!MidiEngine::shouldCoalesceModAmount(500000, 0, 50), "huge elapsed since epoch should send now");
        }

        beginTest("juce::uint32 millisecond-counter wraparound is handled safely");
        {
            // now just past the wrap; lastSendTime just before it -- true elapsed is small.
            juce::uint32 lastSendTime = 4294967290u; // near UINT32_MAX
            juce::uint32 now          = 10u;         // wrapped around
            expect(MidiEngine::shouldCoalesceModAmount(now, lastSendTime, 50),
                   "wrapped elapsed (~16ms) should still coalesce, not read as a huge gap");
        }

        beginTest("A rapid burst of drag values only needs to send the LATEST once the window passes");
        {
            // Simulates a fast max->min drag: many calls within the throttle window, all but
            // the last should coalesce; once the window has passed, the pending flush sends
            // only the final settled value -- never an intermediate one.
            juce::uint32 lastSendTime = 1000;
            std::vector<int> dragValues = { 60, 45, 30, 15, 0, -15, -30, -45, -63 };
            int lastCoalescedValue = 0;
            int sentCount = 0;
            for (size_t i = 0; i < dragValues.size(); ++i)
            {
                juce::uint32 now = 1000 + (juce::uint32)(i * 5); // 5ms apart, well within 50ms window
                if (MidiEngine::shouldCoalesceModAmount(now, lastSendTime, 50))
                    lastCoalescedValue = dragValues[i];
                else
                    { lastSendTime = now; ++sentCount; lastCoalescedValue = dragValues[i]; }
            }
            expectEquals(sentCount, 0, "the whole fast burst should stay inside one throttle window");
            expectEquals(lastCoalescedValue, -63, "the final drag value must be the one that eventually gets flushed");
        }
    }
};

static ModAmountCoalesceTest modAmountCoalesceTestInstance;

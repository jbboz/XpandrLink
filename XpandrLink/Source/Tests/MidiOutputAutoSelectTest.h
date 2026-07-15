/*
  MidiOutputAutoSelectTest.h
  Unit test for MIDI output auto-selection, added after a user question: MIDI input auto-
  detects the synth (the first port to send Oberheim SysEx), but output was previously
  always manual, even on a fresh install with no saved settings.

  MidiEngine::chooseAutoOutput(detectedInputName, availableOutputs) is the pure decision
  function: only picks an output when it's an unambiguous guess (name-matches the detected
  input, or is the only output that exists), never when it would be a real guess among
  multiple unrelated candidates.
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"

class MidiOutputAutoSelectTest : public juce::UnitTest
{
public:
    MidiOutputAutoSelectTest() : juce::UnitTest("MIDI Output Auto-Select", "MidiEngine") {}

    void runTest() override
    {
        beginTest("Output name matches the detected input name -> picked");
        {
            juce::StringArray outputs { "USB MIDI Interface", "IAC Driver Bus 1" };
            expectEquals(MidiEngine::chooseAutoOutput("USB MIDI Interface", outputs),
                         juce::String("USB MIDI Interface"));
        }

        beginTest("Exactly one output exists at all, no name match -> picked (unambiguous)");
        {
            juce::StringArray outputs { "MOTU Fastlane USB MIDI - Port 2" };
            expectEquals(MidiEngine::chooseAutoOutput("MOTU Fastlane USB MIDI - Port 1", outputs),
                         juce::String("MOTU Fastlane USB MIDI - Port 2"));
        }

        beginTest("Multiple outputs, none name-matching -> no guess (empty)");
        {
            juce::StringArray outputs { "Interface A Out 1", "Interface B Out 1", "Interface C Out 1" };
            expect(MidiEngine::chooseAutoOutput("Interface A In 1", outputs).isEmpty(),
                   "ambiguous among 3 unrelated outputs -- must not guess");
        }

        beginTest("No outputs available at all -> empty");
        {
            juce::StringArray outputs;
            expect(MidiEngine::chooseAutoOutput("Any Synth Input", outputs).isEmpty());
        }

        beginTest("Name match preferred even when multiple outputs exist");
        {
            juce::StringArray outputs { "Some Other Interface", "Xpander MIDI", "Yet Another Port" };
            expectEquals(MidiEngine::chooseAutoOutput("Xpander MIDI", outputs),
                         juce::String("Xpander MIDI"));
        }
    }
};

static MidiOutputAutoSelectTest midiOutputAutoSelectTestInstance;

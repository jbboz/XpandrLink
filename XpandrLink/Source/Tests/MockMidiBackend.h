/*
  MockMidiBackend.h

  Test-only IMidiBackend implementation (code-quality plan Phase 3). Records every message passed
  to send() plus open/close state, with no real device enumeration -- open() always succeeds, since
  a mock has no real devices to match a name against. Lets tests drive MidiEngine's real send path
  (page-select/param/mod-matrix/patch-dump queuing, the burst-drag mod-amount coalescing throttle)
  and inspect exactly what would have gone out over the wire, without a real MIDI port.
*/
#pragma once
#include <JuceHeader.h>
#include <vector>
#include "../Engine/IMidiBackend.h"

class MockMidiBackend : public IMidiBackend
{
public:
    bool isOpenFlag = false;
    juce::String openedName;
    std::vector<juce::MidiMessage> sentMessages;

    bool open(const juce::String& name) override { isOpenFlag = true; openedName = name; return true; }
    void close() override { isOpenFlag = false; openedName = {}; }
    bool isOpen() const override { return isOpenFlag; }
    juce::String getName() const override { return openedName; }
    juce::String getIdentifier() const override { return openedName; }
    void send(const juce::MidiMessage& m) override { sentMessages.push_back(m); }
};

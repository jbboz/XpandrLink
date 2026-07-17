/*
  IMidiBackend.h

  Code-quality plan Phase 3 (docs/plans/2026-07-16-code-quality-improvement-plan.md): a thin seam
  between MidiEngine's send-path logic and the concrete JUCE MIDI output device, so tests can drive
  the real send path (page-select/param/mod-matrix/patch-dump queuing, the burst-drag mod-amount
  coalescing throttle) without a real MIDI port. Deliberately scoped to OUTPUT only -- the receive
  path is already directly testable via MidiEngine::processIncomingMessageForTest() (Phase 2.5), and
  input device management goes through juce::AudioDeviceManager's own multi-device callback system,
  a much larger and separately-testable subsystem this phase does not touch.

  JuceMidiBackend wraps exactly the juce::MidiOutput calls MidiEngine used to make directly
  (enumerate-by-name, openDevice, sendMessageNow) with byte-for-byte identical behavior --
  JuceMidiBackend::send() remains a direct, synchronous sendMessageNow with no buffering, matching
  MidiEngine's existing send-queue pacing design, which assumes it blocks.

  JuceMidiBackend is deliberately NOT internally thread-safe. It relies entirely on MidiEngine's own
  call-site locking discipline (see ADR-007, docs/adr/007-activeoutput-locking-invariant.md) being
  preserved exactly -- the same way the raw std::unique_ptr<juce::MidiOutput> it replaces did.
*/
#pragma once
#include <JuceHeader.h>

class IMidiBackend
{
public:
    virtual ~IMidiBackend() = default;

    // Opens the named output device, closing any currently-open one first. Returns true on success.
    virtual bool open(const juce::String& deviceName) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Display name / JUCE device identifier of the currently-open device, or "" if closed.
    virtual juce::String getName() const = 0;
    virtual juce::String getIdentifier() const = 0;

    // Synchronous, blocking send -- no buffering. Callers gate this on isOpen() themselves,
    // matching the existing `if (activeOutput) ...` convention this replaces.
    virtual void send(const juce::MidiMessage& message) = 0;
};

class JuceMidiBackend : public IMidiBackend
{
public:
    bool open(const juce::String& deviceName) override
    {
        output.reset();
        for (auto& d : juce::MidiOutput::getAvailableDevices())
        {
            if (d.name == deviceName)
            {
                output = juce::MidiOutput::openDevice(d.identifier);
                return output != nullptr;
            }
        }
        return false;
    }

    void close() override { output.reset(); }
    bool isOpen() const override { return output != nullptr; }
    juce::String getName() const override { return output ? output->getName() : juce::String(); }
    juce::String getIdentifier() const override { return output ? output->getIdentifier() : juce::String(); }

    void send(const juce::MidiMessage& message) override
    {
        if (output) output->sendMessageNow(message);
    }

private:
    std::unique_ptr<juce::MidiOutput> output;
};

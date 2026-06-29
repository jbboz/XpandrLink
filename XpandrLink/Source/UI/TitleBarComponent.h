#pragma once
#include <JuceHeader.h>
#include <functional>
#include <atomic>
#include "HardwareButtonLookAndFeel.h"

// LED activity indicator — flashes on MIDI in/out activity.
// Moved here from EditorTabComponent.h (was file-scope there).
class MidiActivityIndicator : public juce::Component, public juce::Timer
{
public:
    void paint(juce::Graphics& g) override {
        g.setColour(active ? activeColour : juce::Colours::darkgrey.darker());
        g.fillEllipse(getLocalBounds().reduced(2).toFloat());
    }
    void flash() { active = true; repaint(); startTimer(100); }
    void timerCallback() override { active = false; stopTimer(); repaint(); }
    void setColour(juce::Colour c) { activeColour = c; }
private:
    bool active = false;
    juce::Colour activeColour = juce::Colours::green;
};

class TitleBarComponent : public juce::Component
{
public:
    TitleBarComponent();
    ~TitleBarComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void parentHierarchyChanged() override;

    // Setters — parent pushes state in
    void setPatchName(const juce::String& name);
    void setProgramNumber(int n);
    void flashMidiLed(bool isInput);   // thread-safe; may be called from MIDI thread

    // Queries — parent reads position for browser panel placement
    juce::Rectangle<int> getVfdNameBounds() const;  // in parent coordinates

    // Callbacks — parent wires these in its constructor
    std::function<void()>    onPrevClicked;
    std::function<void()>    onNextClicked;
    std::function<void()>    onLibraryClicked;
    std::function<void(int)> onPatchNumberEdited;

private:
    void hideStandaloneOptionsButton();

    juce::Label       patchNameLabel, patchNumberLabel;
    juce::TextButton  btnPrev{"<"}, btnNext{">"}, btnLibrary;
    juce::Label       midiInLabel, midiOutLabel;
    MidiActivityIndicator ledIn, ledOut;
    std::atomic<bool> midiInFlashPending  { false };
    std::atomic<bool> midiOutFlashPending { false };

    // MIDI activity LEDs are standalone-only. In a DAW the host owns MIDI routing,
    // so the indicators are omitted (and flashMidiLed becomes a no-op).
    const bool showMidiActivity_ { juce::JUCEApplicationBase::isStandaloneApp() };

    juce::Rectangle<int> vfdNameArea, vfdNumberArea;

    HardwareButtonLookAndFeel hwButtonLF_;
    bool optionsButtonHidden_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitleBarComponent)
};

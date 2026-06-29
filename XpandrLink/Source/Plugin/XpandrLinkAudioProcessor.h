#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "../UI/ModAssignmentLogic.h"
#include "XpandrLinkParameters.h"

class XpandrLinkAudioProcessor : public juce::AudioProcessor,
                                  public juce::AudioProcessorParameter::Listener,
                                  public MidiEngine::Listener
{
public:
    XpandrLinkAudioProcessor();
    ~XpandrLinkAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ignoreUnused (buffer);
    }

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    MidiEngine&            getMidiEngine()            { return midiEngine; }
    ModAssignmentLogic&    getModAssignmentLogic()    { return modAssignmentLogic; }

    //==============================================================================
    // AudioProcessorParameter::Listener — host automation → hardware
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

    //==============================================================================
    // MidiEngine::Listener — hardware → host automation
    void onMidiActivity (bool) override {}
    void onPatchReceived (const std::vector<uint8_t>&, int) override {}
    void onParameterChangedFromHardware (int page, int paramCol, int value, bool isDelta, bool isButton) override;
    void onStatusUpdate (const juce::String&) override {}
    void onParameterSentToSynth (int page, int paramCol, int value, bool isButton) override;

private:
    // Construction order is critical: midiEngine first, paramBridge second.
    // paramBridge calls addParameter() which must happen before any host queries.
    ModAssignmentLogic          modAssignmentLogic;
    MidiEngine                  midiEngine;
    XpandrLinkParameterBridge   paramBridge;

    // Set while parameterValueChanged dispatches sendParameterToSynth so that
    // onParameterSentToSynth doesn't re-notify the host (which would look like a
    // manual override and disable the automation lane in Ableton).
    bool fromHostAutomation = false;

    // Set while we broadcastParameterChange to push a host-automation value to the
    // editor UI. Prevents onParameterChangedFromHardware from calling setValueNotifyingHost
    // again (host already knows — it sent us the value).
    bool updatingUIFromHost = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (XpandrLinkAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XpandrLinkAudioProcessor)
};

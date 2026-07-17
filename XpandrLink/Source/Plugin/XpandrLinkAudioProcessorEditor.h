#pragma once
#include <JuceHeader.h>
#include "XpandrLinkAudioProcessor.h"
#include "../MainComponent.h"

class XpandrLinkAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit XpandrLinkAudioProcessorEditor (XpandrLinkAudioProcessor&);
    ~XpandrLinkAudioProcessorEditor() override;

    void resized() override;

private:
    static constexpr int kDesignW = 1280;
    static constexpr int kDesignH = 800;

    XpandrLinkAudioProcessor& processor;
    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XpandrLinkAudioProcessorEditor)
};

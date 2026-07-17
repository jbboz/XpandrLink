#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"

// Popup editor for changing the amount of an existing mod routing (-63…+63).
class ModAmountEditor : public juce::Component
{
public:
    std::function<void(int)> onAmountChanged;

    explicit ModAmountEditor(int currentAmount)
    {
        auto theme = ThemeData::getHardwareTheme();

        lbl.setText("Amount", juce::dontSendNotification);
        lbl.setFont(juce::Font(juce::FontOptions(11.0f)));
        lbl.setColour(juce::Label::textColourId, theme.textLabel);
        lbl.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lbl);

        slider.setRange(-63, 63, 1);
        slider.setValue(currentAmount, juce::dontSendNotification);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 18);
        slider.setColour(juce::Slider::thumbColourId, theme.knobIndicatorActive);
        slider.setColour(juce::Slider::trackColourId, theme.groupOutline);
        slider.setColour(juce::Slider::textBoxTextColourId, theme.textValue);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, theme.vfdBackground);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.onValueChange = [this] {
            if (onAmountChanged) onAmountChanged((int)slider.getValue());
        };
        addAndMakeVisible(slider);

        setSize(200, 46);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4);
        lbl.setBounds(b.removeFromTop(14));
        slider.setBounds(b);
    }

private:
    juce::Label  lbl;
    juce::Slider slider;
};

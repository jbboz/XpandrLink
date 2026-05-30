/*
  ModSourceButton.h
  Clicking this enters "Routing Mode" and shows an amount picker.
*/
#pragma once
#include <JuceHeader.h>
#include "ModAssignmentLogic.h"
#include "OberheimLookAndFeel.h"
#include "ThemeData.h"

// ---------------------------------------------------------------------------
// AmountPicker — shown in a CallOutBox when routing mode is activated.
// Lets the user set amount (-63…+63) before clicking a destination.
// ---------------------------------------------------------------------------
class AmountPicker : public juce::Component
{
public:
    explicit AmountPicker(ModAssignmentLogic* modLogic = nullptr) : modAssignmentLogic(modLogic)
    {
        auto theme = ThemeData::getHardwareTheme();

        lblAmount.setText("Amount", juce::dontSendNotification);
        lblAmount.setFont(juce::Font(juce::FontOptions(11.0f)));
        lblAmount.setColour(juce::Label::textColourId, theme.textLabel);
        lblAmount.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblAmount);

        slider.setRange(-63, 63, 1);
        int initAmt = modLogic ? modLogic->getPendingAmount() : 63;
        slider.setValue(initAmt, juce::dontSendNotification);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 18);
        slider.setColour(juce::Slider::thumbColourId, theme.knobIndicatorActive);
        slider.setColour(juce::Slider::trackColourId, theme.groupOutline);
        slider.setColour(juce::Slider::textBoxTextColourId, theme.textValue);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, theme.vfdBackground);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.onValueChange = [this] {
            if (modAssignmentLogic) modAssignmentLogic->setPendingAmount((int)slider.getValue());
        };
        addAndMakeVisible(slider);

        lblHint.setText("then click a destination", juce::dontSendNotification);
        lblHint.setFont(juce::Font(juce::FontOptions(9.0f)));
        lblHint.setColour(juce::Label::textColourId, theme.textLabel.withAlpha(0.6f));
        lblHint.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lblHint);

        setSize(200, 60);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4);
        lblAmount.setBounds(b.removeFromTop(14));
        slider.setBounds(b.removeFromTop(26));
        lblHint.setBounds(b);
    }

private:
    ModAssignmentLogic* modAssignmentLogic = nullptr;
    juce::Label  lblAmount;
    juce::Slider slider;
    juce::Label  lblHint;
};

// ---------------------------------------------------------------------------

class ModSourceButton : public juce::Button
{
public:
    ModSourceButton(const juce::String& name, int sourceIndex, ModAssignmentLogic* modLogic = nullptr)
        : juce::Button(name), mySourceIndex(sourceIndex), modAssignmentLogic(modLogic)
    {
        setTooltip("Click to route " + name + " to a destination");
    }

    void paintButton(juce::Graphics& g, bool /*isMouseOver*/, bool /*isButtonDown*/) override
    {
        auto& lf = dynamic_cast<OberheimLookAndFeel&>(getLookAndFeel());
        auto theme = lf.getTheme();
        auto bounds = getLocalBounds().toFloat();

        bool isActive = modAssignmentLogic && modAssignmentLogic->isRouting()
                        && modAssignmentLogic->getCurrentSourceName() == getName();

        if (isActive) g.setColour(theme.textValue.withAlpha(0.3f));
        else g.setColour(juce::Colour(0xff222222));
        g.fillRect(bounds);

        g.setColour(isActive ? theme.textValue : theme.groupOutline);
        g.drawRect(bounds, 1.0f);

        g.setColour(isActive ? theme.textValue : theme.textLabel);
        g.setFont(10.0f);
        g.drawText(getName(), bounds, juce::Justification::centred);
    }

    void clicked() override
    {
        if (!modAssignmentLogic) return;
        if (modAssignmentLogic->isRouting() && modAssignmentLogic->getCurrentSourceName() == getName())
        {
            modAssignmentLogic->cancelRouting();
        }
        else
        {
            modAssignmentLogic->setActiveSource(mySourceIndex, getName());
            auto screenBounds = localAreaToGlobal(getLocalBounds());
            auto* ml = modAssignmentLogic;
            juce::MessageManager::callAsync([screenBounds, ml]
            {
                juce::CallOutBox::launchAsynchronously(
                    std::make_unique<AmountPicker>(ml), screenBounds, nullptr);
            });
        }
        if (auto* top = getTopLevelComponent()) top->repaint();
    }

private:
    int mySourceIndex;
    ModAssignmentLogic* modAssignmentLogic = nullptr;
};
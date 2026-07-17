/*
  RandomizerPanel.h
  Bottom-pane UI for the smart patch randomizer + nudge (TASK-0).

  Per-section toggles + Amount slider + Nudge / Randomize / Revert buttons.
  Pure UI: it just gathers a PatchRandomizer::Config and fires callbacks;
  EditorTabComponent does the actual rolling, applying and hardware send.
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"
#include "HardwareButtonLookAndFeel.h"
#include "../Data/PatchRandomizer.h"

class RandomizerPanel : public juce::Component
{
public:
    std::function<void()> onNudge;
    std::function<void()> onRandomize;
    std::function<void()> onRevert;

    RandomizerPanel()
    {
        auto theme = ThemeData::getHardwareTheme();

        titleLabel_.setText("RANDOMIZER", juce::dontSendNotification);
        titleLabel_.setColour(juce::Label::textColourId, theme.textLabel);
        titleLabel_.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        addAndMakeVisible(titleLabel_);

        scopeLabel_.setText("SCOPE", juce::dontSendNotification);
        scopeLabel_.setColour(juce::Label::textColourId, theme.textLabel.withAlpha(0.7f));
        scopeLabel_.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        addAndMakeVisible(scopeLabel_);

        // Per-section toggle buttons (all enabled by default).
        for (int i = 0; i < PatchRandomizer::NumSections; ++i)
        {
            auto* b = new juce::TextButton(PatchRandomizer::sectionName(i));
            b->setClickingTogglesState(true);
            b->setToggleState(false, juce::dontSendNotification);  // scopes off by default
            b->setLookAndFeel(&lf_);
            addAndMakeVisible(b);
            sectionButtons_.add(b);
        }

        // Amount slider 1..100.
        amountLabel_.setText("AMOUNT", juce::dontSendNotification);
        amountLabel_.setColour(juce::Label::textColourId, theme.textLabel.withAlpha(0.7f));
        amountLabel_.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        addAndMakeVisible(amountLabel_);

        amountSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
        amountSlider_.setRange(1.0, 100.0, 1.0);
        amountSlider_.setValue(35.0, juce::dontSendNotification);
        amountSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 22);
        amountSlider_.setColour(juce::Slider::trackColourId,         theme.knobIndicatorActive.withAlpha(0.6f));
        amountSlider_.setColour(juce::Slider::thumbColourId,         theme.knobIndicatorActive);
        amountSlider_.setColour(juce::Slider::backgroundColourId,    juce::Colour(0xff2a2a2a));
        amountSlider_.setColour(juce::Slider::textBoxTextColourId,   theme.textValue);
        amountSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(amountSlider_);

        // Musical-safety toggle.
        safetyButton_.setButtonText("SAFE");
        safetyButton_.setClickingTogglesState(true);
        safetyButton_.setToggleState(true, juce::dontSendNotification);
        safetyButton_.setLookAndFeel(&lf_);
        addAndMakeVisible(safetyButton_);

        setupAction(nudgeButton_,     "NUDGE");
        setupAction(randomizeButton_, "RANDOMIZE");
        setupAction(revertButton_,    "UNDO");
        revertButton_.setEnabled(false);

        nudgeButton_.onClick     = [this] { if (onNudge)     onNudge();     };
        randomizeButton_.onClick = [this] { if (onRandomize) onRandomize(); };
        revertButton_.onClick    = [this] { if (onRevert)    onRevert();    };
    }

    ~RandomizerPanel() override
    {
        // Detach the custom LookAndFeel before destruction — safety currently relies
        // on lf_ being declared before the buttons; detaching makes it order-proof.
        for (auto* b : sectionButtons_) b->setLookAndFeel(nullptr);
        safetyButton_.setLookAndFeel(nullptr);
        nudgeButton_.setLookAndFeel(nullptr);
        randomizeButton_.setLookAndFeel(nullptr);
        revertButton_.setLookAndFeel(nullptr);
    }

    // Build a Config snapshot from the current control state.
    PatchRandomizer::Config getConfig() const
    {
        PatchRandomizer::Config c;
        for (int i = 0; i < PatchRandomizer::NumSections; ++i)
            c.sectionEnabled[i] = sectionButtons_[i]->getToggleState();
        c.amount        = (int) amountSlider_.getValue();
        c.musicalSafety = safetyButton_.getToggleState();
        return c;
    }

    void setRevertEnabled(bool enabled) { revertButton_.setEnabled(enabled); }

    void paint(juce::Graphics& g) override
    {
        auto theme = ThemeData::getHardwareTheme();
        g.fillAll(theme.mainBackground);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12, 8);

        titleLabel_.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);

        // Scope row: label + 9 section toggles.
        auto scopeRow = area.removeFromTop(28);
        scopeLabel_.setBounds(scopeRow.removeFromLeft(54));
        const int n = sectionButtons_.size();
        const int gap = 4;
        int bw = (scopeRow.getWidth() - (n - 1) * gap) / n;
        for (int i = 0; i < n; ++i)
        {
            sectionButtons_[i]->setBounds(scopeRow.removeFromLeft(bw));
            if (i < n - 1) scopeRow.removeFromLeft(gap);
        }

        area.removeFromTop(10);

        // Amount row: label + slider + SAFE toggle.
        auto amtRow = area.removeFromTop(26);
        amountLabel_.setBounds(amtRow.removeFromLeft(54));
        safetyButton_.setBounds(amtRow.removeFromRight(60));
        amtRow.removeFromRight(8);
        amountSlider_.setBounds(amtRow);

        area.removeFromTop(12);

        // Action row: Nudge | Randomize | Revert.
        auto actRow = area.removeFromTop(juce::jmin(34, area.getHeight()));
        const int agap = 8;
        int aw = (actRow.getWidth() - 2 * agap) / 3;
        nudgeButton_.setBounds(actRow.removeFromLeft(aw));     actRow.removeFromLeft(agap);
        randomizeButton_.setBounds(actRow.removeFromLeft(aw)); actRow.removeFromLeft(agap);
        revertButton_.setBounds(actRow);
    }

private:
    void setupAction(juce::TextButton& b, const juce::String& text)
    {
        b.setButtonText(text);
        b.setLookAndFeel(&lf_);
        addAndMakeVisible(b);
    }

    // Declared first so it outlives every button below (members destruct in reverse order).
    HardwareButtonLookAndFeel lf_;
    juce::Label  titleLabel_, scopeLabel_, amountLabel_;
    juce::OwnedArray<juce::TextButton> sectionButtons_;
    juce::Slider amountSlider_;
    juce::TextButton safetyButton_;
    juce::TextButton nudgeButton_, randomizeButton_, revertButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomizerPanel)
};

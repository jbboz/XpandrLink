#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "ThemeData.h"
#include "HardwareButtonLookAndFeel.h"

// MIDI port configuration pane — the app's MIDI I/O UI in every build
// (the standalone wrapper's redundant "Options" dialog is hidden).
// SYNTH is a read-only connection LED: lit once a port sends Oberheim SysEx (i.e. an
// Xpander/Matrix-12 is detected). The model can't be told from the stream, so it's a
// plain presence indicator, not a model readout.
class MidiSettingsPanel : public juce::Component, private juce::ListBoxModel
{
    // Static (non-flashing) presence LED, matching the title-bar MIDI LED style.
    struct SynthLed : public juce::Component, public juce::SettableTooltipClient
    {
        bool lit = false;
        void paint(juce::Graphics& g) override
        {
            g.setColour(lit ? juce::Colour(0xff00fc2e) : juce::Colours::darkgrey.darker());
            g.fillEllipse(getLocalBounds().reduced(2).toFloat());
        }
    };
public:
    std::function<void()> onChanged;

    MidiSettingsPanel(MidiEngine& engine) : midiEngine_(engine)
    {
        auto& theme = ThemeData::getHardwareTheme();

        headerLabel_.setText("MIDI INPUT", juce::dontSendNotification);
        headerLabel_.setJustificationType(juce::Justification::centredLeft);
        headerLabel_.setColour(juce::Label::textColourId, theme.textLabel);
        headerLabel_.setFont(ThemeData::getVfdFont(13.0f));
        addAndMakeVisible(headerLabel_);

        refreshBtn_.setButtonText("Refresh");
        refreshBtn_.setLookAndFeel(&hwLF_);
        refreshBtn_.onClick = [this] { refresh(); };
        addAndMakeVisible(refreshBtn_);

        inputList_.setModel(this);
        inputList_.setRowHeight(22);
        inputList_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a1a1a));
        inputList_.setColour(juce::ListBox::outlineColourId, ThemeData::getHardwareTheme().vfdBorder);
        inputList_.setOutlineThickness(1);
        addAndMakeVisible(inputList_);

        outputLabel_.setText("MIDI OUTPUT", juce::dontSendNotification);
        outputLabel_.setJustificationType(juce::Justification::centredLeft);
        outputLabel_.setColour(juce::Label::textColourId, theme.textLabel);
        outputLabel_.setFont(ThemeData::getVfdFont(13.0f));
        addAndMakeVisible(outputLabel_);

        outputBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a1a));
        outputBox_.setColour(juce::ComboBox::textColourId, theme.textValue);
        outputBox_.setColour(juce::ComboBox::outlineColourId, theme.vfdBorder);
        outputBox_.onChange = [this] {
            auto name = outputBox_.getText();
            if (name.isNotEmpty())
                midiEngine_.setMidiOutput(name);
            if (onChanged) onChanged();
        };
        addAndMakeVisible(outputBox_);

        idLabel_.setText("SYNTH", juce::dontSendNotification);
        idLabel_.setJustificationType(juce::Justification::centredLeft);
        idLabel_.setColour(juce::Label::textColourId, theme.textLabel);
        idLabel_.setFont(ThemeData::getVfdFont(11.0f));
        addAndMakeVisible(idLabel_);

        synthLed_.setTooltip("Lit when an Xpander/Matrix-12 has been detected on a MIDI "
                             "input (a port has sent Oberheim SysEx). Turn a knob on the "
                             "synth if this stays unlit.");
        addAndMakeVisible(synthLed_);

        statusLabel_.setJustificationType(juce::Justification::centredLeft);
        statusLabel_.setFont(ThemeData::getVfdFont(11.0f));
        addAndMakeVisible(statusLabel_);

        refresh();
    }

    void paint(juce::Graphics& g) override
    {
        // SYNTH connection readout lives in a VFD box (same treatment as the Page 2
        // labeled dropdowns) — vfdDim/bright-green text is only readable against this
        // dark background, not the bare panel background.
        auto& theme = ThemeData::getHardwareTheme();
        g.setColour(theme.vfdBackground);
        g.fillRect(synthBoxArea_);
        g.setColour(theme.groupOutline);
        g.drawRect(synthBoxArea_, 1);
    }

    ~MidiSettingsPanel() override
    {
        refreshBtn_.setLookAndFeel(nullptr);
    }

    void refresh()
    {
        // Repopulate inputs
        availableInputs_ = midiEngine_.getMidiInputs();
        inputList_.updateContent();

        // Restore checkmarks for enabled inputs
        auto enabled = midiEngine_.getEnabledMidiInputNames();
        for (int row = 0; row < availableInputs_.size(); ++row)
        {
            bool isEnabled = enabled.contains(availableInputs_[row]);
            inputList_.selectRow(row, false, !isEnabled);
        }
        // Use repaint instead of selection state for checkmark rendering
        inputList_.repaint();

        // Repopulate output
        outputBox_.clear(juce::dontSendNotification);
        auto outputs = midiEngine_.getMidiOutputs();
        for (int i = 0; i < outputs.size(); ++i)
            outputBox_.addItem(outputs[i], i + 1);
        auto currentOut = midiEngine_.getCurrentMidiOutputName();
        outputBox_.setText(currentOut, juce::dontSendNotification);

        // Synth presence LED: lit once any port has sent Oberheim SysEx (getSynthInputName
        // is non-empty). Not a model readout — the SysEx unit-ID byte can't distinguish
        // Xpander from Matrix-12 (both transmit 0x02).
        synthLed_.lit = midiEngine_.getSynthInputName().isNotEmpty();
        synthLed_.repaint();

        auto& theme = ThemeData::getHardwareTheme();
        if (synthLed_.lit)
        {
            statusLabel_.setText("Connected", juce::dontSendNotification);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff00fc2e));
        }
        else
        {
            statusLabel_.setText("Not Connected", juce::dontSendNotification);
            statusLabel_.setColour(juce::Label::textColourId, theme.vfdDim);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8, 6);

        // Two columns: MIDI INPUT (list) on the left, MIDI OUTPUT + SYNTH on the right
        auto left  = r.removeFromLeft(r.getWidth() / 2);
        r.removeFromLeft(12);
        auto right = r;

        headerLabel_.setBounds(left.removeFromTop(22));
        left.removeFromTop(4);
        inputList_.setBounds(left);

        auto rightTop = right.removeFromTop(22);
        refreshBtn_.setBounds(rightTop.removeFromRight(64));
        rightTop.removeFromRight(4);
        outputLabel_.setBounds(rightTop);
        right.removeFromTop(4);
        outputBox_.setBounds(right.removeFromTop(26).withTrimmedRight(right.getWidth() / 4));

        right.removeFromTop(8);
        auto idRow = right.removeFromTop(26);
        idLabel_.setBounds(idRow.removeFromLeft(46));
        idRow.removeFromLeft(4);

        // VFD box (same ~26px height as the Page 2 labeled dropdowns) housing the LED +
        // status text; painted in paint(). Sized to fit "Not Connected" (the longer of the
        // two states) rather than stretching across the rest of the row.
        auto vfdFont = ThemeData::getVfdFont(11.0f);
        int textW = (int) std::ceil(vfdFont.getStringWidthFloat("Not Connected"));
        int boxW = 16 /*LED cell*/ + 4 /*gap*/ + textW + 12 /*inner padding*/;
        synthBoxArea_ = idRow.removeFromLeft(boxW);
        auto boxInner = synthBoxArea_.reduced(6, 0);
        // Match the title-bar MIDI IN/OUT LEDs: 10x10, centred in its cell.
        synthLed_.setBounds(boxInner.removeFromLeft(16).withSizeKeepingCentre(10, 10));
        boxInner.removeFromLeft(4);
        statusLabel_.setBounds(boxInner);
    }

private:
    // ---- ListBoxModel -------------------------------------------------------
    int getNumRows() override { return availableInputs_.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool /*selected*/) override
    {
        auto& theme = ThemeData::getHardwareTheme();
        if (row % 2 == 0)
            g.fillAll(juce::Colour(0xff222222));

        auto enabled = midiEngine_.getEnabledMidiInputNames();
        bool isEnabled = row < availableInputs_.size()
                         && enabled.contains(availableInputs_[row]);

        // Checkmark column
        g.setColour(isEnabled ? theme.textValue : theme.vfdDim);
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(isEnabled ? juce::String::fromUTF8("\xe2\x9c\x93") : " ",
                   4, 0, 20, h, juce::Justification::centredLeft);

        // Port name
        g.setColour(isEnabled ? theme.textValue : theme.textLabel.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        if (row < availableInputs_.size())
            g.drawText(availableInputs_[row], 24, 0, w - 28, h, juce::Justification::centredLeft);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (row < 0 || row >= availableInputs_.size()) return;
        auto name = availableInputs_[row];
        auto enabled = midiEngine_.getEnabledMidiInputNames();
        if (enabled.contains(name))
            midiEngine_.removeMidiInput(name);
        else
            midiEngine_.addMidiInput(name);
        inputList_.repaint();
        if (onChanged) onChanged();
    }

    // -------------------------------------------------------------------------
    MidiEngine&  midiEngine_;
    juce::StringArray availableInputs_;

    juce::Label          headerLabel_, outputLabel_, idLabel_, statusLabel_;
    juce::TextButton     refreshBtn_;
    juce::ListBox        inputList_;
    juce::ComboBox       outputBox_;
    SynthLed             synthLed_;
    juce::Rectangle<int> synthBoxArea_;

    HardwareButtonLookAndFeel hwLF_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiSettingsPanel)
};

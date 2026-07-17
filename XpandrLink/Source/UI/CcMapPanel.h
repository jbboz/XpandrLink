#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "../Data/XpanderData.h"
#include "ThemeData.h"
#include "HardwareComponents.h"

// CC automation table: maps MIDI CC numbers (0-127) to Xpander parameters.
// Incoming CC from non-synth inputs is scaled to the param range and broadcast
// as a hardware parameter event — does not echo back to the synth.
class CcMapPanel : public juce::Component, private juce::ListBoxModel
{
public:
    std::function<void()> onChanged;   // fired after any mapping change — call saveSettings()

    CcMapPanel(MidiEngine& engine) : midiEngine_(engine)
    {
        // Build the shared item list once: a plain "unmapped" row + every param, prefixed
        // with its group (e.g. "VCF Res", "ENV 1 Amp", "LFO 2 Speed"). Many param names
        // are single generic words ("Res", "Amp", "Freq", "Speed") reused across several
        // modules -- the bare name alone doesn't say which one, e.g. "Res" could easily be
        // mistaken for something other than VCF resonance. The group prefix disambiguates.
        paramItems_.add("unmapped");
        for (auto& p : XpanderParams)
        {
            juce::String label = p.group;
            if (p.name.isNotEmpty())
                label = label.isNotEmpty() ? (label + " " + p.name) : p.name;
            paramItems_.add(label);
        }

        clearAllBtn_.setButtonText("Clear All");
        clearAllBtn_.onClick = [this] {
            for (int i = 0; i < 128; ++i)
                midiEngine_.setCcMap(i, -1);
            listBox_.updateContent();
            if (onChanged) onChanged();
        };
        addAndMakeVisible(clearAllBtn_);

        headerLabel_.setText("MIDI CC     PARAMETER", juce::dontSendNotification);
        headerLabel_.setJustificationType(juce::Justification::centredLeft);
        headerLabel_.setColour(juce::Label::textColourId, ThemeData::getHardwareTheme().textLabel);
        headerLabel_.setFont(ThemeData::getVfdFont(13.0f));
        addAndMakeVisible(headerLabel_);

        listBox_.setModel(this);
        listBox_.setRowHeight(22);
        listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
        addAndMakeVisible(listBox_);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8, 6);
        auto topBar = r.removeFromTop(24);
        clearAllBtn_.setBounds(topBar.removeFromRight(80));
        headerLabel_.setBounds(topBar);
        r.removeFromTop(4);
        listBox_.setBounds(r);
    }

    // Called by EditorTabComponent after construction to pre-select saved mappings.
    void refresh() { listBox_.updateContent(); }

private:
    // ---- ListBoxModel -------------------------------------------------------
    int getNumRows() override { return 128; }

    void paintListBoxItem(int /*row*/, juce::Graphics& /*g*/, int /*w*/, int /*h*/, bool /*sel*/) override {}

    juce::Component* refreshComponentForRow(int row, bool /*selected*/, juce::Component* existing) override
    {
        auto* comp = dynamic_cast<RowComp*>(existing);
        if (comp == nullptr) comp = new RowComp(*this);
        comp->setup(row, midiEngine_.getCcMap(row));
        return comp;
    }

    // ---- Row component -------------------------------------------------------
    class RowComp : public juce::Component
    {
    public:
        explicit RowComp(CcMapPanel& owner) : owner_(owner)
        {
            ccLabel_.setJustificationType(juce::Justification::centredRight);
            ccLabel_.setColour(juce::Label::textColourId, ThemeData::getHardwareTheme().textLabel);
            ccLabel_.setFont(ThemeData::getVfdFont(12.0f));
            addAndMakeVisible(ccLabel_);

            combo_.setItems(owner_.paramItems_);
            combo_.onChange = [this](int selectedIndex) {
                // index 0 = "unmapped"; 1.. map to XpanderParams[index-1].
                int idx     = selectedIndex - 1;
                int paramId = (idx >= 0 && idx < (int)XpanderParams.size())
                            ? XpanderParams[(size_t)idx].id : -1;
                owner_.midiEngine_.setCcMap(row_, paramId);
                if (owner_.onChanged) owner_.onChanged();
            };
            addAndMakeVisible(combo_);
        }

        void setup(int row, int currentParamId)
        {
            row_ = row;
            ccLabel_.setText("CC " + juce::String(row), juce::dontSendNotification);

            int selIndex = 0;  // default: "unmapped"
            if (currentParamId >= 0)
            {
                for (int i = 0; i < (int)XpanderParams.size(); ++i)
                {
                    if (XpanderParams[(size_t)i].id == currentParamId)
                    {
                        selIndex = i + 1;
                        break;
                    }
                }
            }
            combo_.setSelectedIndex(selIndex);  // does not fire onChange — safe during setup
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced(2, 1);
            ccLabel_.setBounds(b.removeFromLeft(56));
            b.removeFromLeft(4);
            combo_.setBounds(b);
        }

        void paint(juce::Graphics& g) override
        {
            auto bg = (row_ % 2 == 0) ? juce::Colour(0xff2a2a2a) : juce::Colour(0xff242424);
            g.fillAll(bg);
        }

    private:
        CcMapPanel& owner_;
        juce::Label    ccLabel_;
        VfdDropdown combo_{ "" };
        int row_ = 0;
    };

    // -------------------------------------------------------------------------
    MidiEngine&       midiEngine_;
    juce::StringArray paramItems_;
    juce::TextButton  clearAllBtn_;
    juce::Label       headerLabel_;
    juce::ListBox     listBox_;
};

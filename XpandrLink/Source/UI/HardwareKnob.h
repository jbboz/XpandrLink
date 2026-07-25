#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"
#include "ModAssignmentLogic.h"
#include "HardwareComponents.h"   // NoTextBoxLookAndFeel, fontStringWidth

// --- VFD KNOB ---
class HardwareKnob : public juce::Slider
{
public:
    HardwareKnob(const juce::String& labelText, ModAssignmentLogic* modLogic = nullptr);
    ~HardwareKnob() override;

    int paramID = -1;
    std::function<void()> onAssignmentCallback;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void paint(juce::Graphics& g) override;

private:
    juce::String name;
    ModAssignmentLogic* modAssignmentLogic = nullptr;
    NoTextBoxLookAndFeel noTextLF;

    struct NumericEntry : public juce::Component
    {
        juce::TextEditor ed;

        NumericEntry(int current, int minV, int maxV, std::function<void(int)> apply)
        {
            ed.setText(juce::String(current));
            ed.selectAll();
            ed.setInputRestrictions(6, "-0123456789");
            ed.setFont(ThemeData::getVfdFont(13.0f));
            addAndMakeVisible(ed);
            setSize(72, 24);

            ed.onReturnKey = [this, minV, maxV, apply]()
            {
                int val = juce::jlimit(minV, maxV, ed.getText().getIntValue());
                apply(val);
                if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                    cb->dismiss();
            };
            ed.onEscapeKey = [this]()
            {
                if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                    cb->dismiss();
            };
        }

        void resized() override { ed.setBounds(getLocalBounds()); }
        void visibilityChanged() override { if (isVisible()) ed.grabKeyboardFocus(); }
    };
};

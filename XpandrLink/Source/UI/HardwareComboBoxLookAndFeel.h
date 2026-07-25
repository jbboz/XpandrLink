#pragma once
#include <JuceHeader.h>
#include "ModAssignmentLogic.h"

// --- COMBOBOX LF (Fixes Double Text) ---
class HardwareComboBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModAssignmentLogic* modLogic = nullptr;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    // Kill the default label that might pop up
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(0.0f)); }
};

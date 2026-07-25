/*
  HardwareComponents.h
  Shared remainder after the Phase 4 / P1 per-class split: NoTextBoxLookAndFeel and
  fontStringWidth are tiny, stateless, and used by more than one of the classes that used
  to live here (HardwareKnob, VfdDropdown, WaveformButton) -- everything else moved to its
  own file pair (HardwareComboBoxLookAndFeel, HardwareKnob, HardwareMenu, VfdPopupList,
  VfdDropdown, WaveformButton).
*/
#pragma once
#include <JuceHeader.h>

// --- SLIDER LF (Kills Text Box) ---
class NoTextBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Label* createSliderTextBox(juce::Slider&) override {
        auto* l = new juce::Label(); l->setVisible(false); return l;
    }
};

inline float fontStringWidth(const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText(f, s, 0.0f, 0.0f);
    return ga.getBoundingBox(0, -1, true).getWidth();
}

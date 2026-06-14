/*
  RndMorphPanel.h
  Hosts RandomizerPanel (left) + MorphPanel (right) side-by-side as a single
  bottom pane (TASK-30). Owns neither child — EditorTabComponent retains
  ownership and all callback wiring; this container only reparents and lays
  them out.
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"
#include "RandomizerPanel.h"
#include "MorphPanel.h"

class RndMorphPanel : public juce::Component
{
public:
    RndMorphPanel(RandomizerPanel& rnd, MorphPanel& morph)
        : rnd_(rnd), morph_(morph)
    {
        addAndMakeVisible(rnd_);
        addAndMakeVisible(morph_);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(ThemeData::getHardwareTheme().mainBackground);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        int half  = area.getWidth() / 2;
        rnd_.setBounds(area.removeFromLeft(half));
        area.removeFromLeft(8);                 // divider gap
        morph_.setBounds(area);
    }

private:
    RandomizerPanel& rnd_;
    MorphPanel&      morph_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RndMorphPanel)
};

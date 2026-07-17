/*
  HardwareButtonLookAndFeel.h
  Matrix-12-style membrane button rendering for the title-bar / nav-bar TextButtons
  (Mod / PAGE2 / RND / prev / next). Dark charcoal raised face with a soft top-light
  bevel + bottom shadow; the lit/active state (driven by the button's toggle state)
  brightens the face and adds a blue accent edge, matching the Original Matrix-12 panel.
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"

class HardwareButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                              const juce::Colour& /*backgroundColour*/,
                              bool isHighlighted, bool isDown) override
    {
        auto theme = ThemeData::getHardwareTheme();
        auto r = b.getLocalBounds().toFloat().reduced(1.0f);
        const float corner = 3.0f;
        const bool  on = b.getToggleState();

        // Raised face — vertical gradient (lighter top → darker bottom).
        juce::Colour top, bot;
        if (isDown)      { top = juce::Colour(0xff161616); bot = juce::Colour(0xff0d0d0d); }
        else if (on)     { top = juce::Colour(0xff3b4c61); bot = juce::Colour(0xff253240); } // lit blue-grey
        else             { top = juce::Colour(0xff37373a); bot = juce::Colour(0xff212123); } // charcoal
        if (isHighlighted && !isDown) { top = top.brighter(0.10f); bot = bot.brighter(0.10f); }

        g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(),
                                               bot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, corner);

        // Bevel: top highlight + bottom shadow give the raised membrane feel.
        if (!isDown)
        {
            g.setColour(juce::Colours::white.withAlpha(on ? 0.16f : 0.09f));
            g.drawLine(r.getX() + 2.5f, r.getY() + 1.3f, r.getRight() - 2.5f, r.getY() + 1.3f, 1.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.drawLine(r.getX() + 2.5f, r.getBottom() - 1.1f, r.getRight() - 2.5f, r.getBottom() - 1.1f, 1.0f);

        // Outer border.
        g.setColour(juce::Colour(0xff0e0e0e));
        g.drawRoundedRectangle(r, corner, 1.0f);

        // Active accent — a thin blue bar along the top edge (matches section headers).
        if (on)
        {
            auto accent = r.reduced(3.0f, 0.0f).withHeight(2.0f).translated(0.0f, 1.5f);
            g.setColour(theme.knobIndicatorActive.withAlpha(0.85f));
            g.fillRoundedRectangle(accent, 1.0f);
        }

        // Disabled — dim the whole face.
        if (!b.isEnabled())
        {
            g.setColour(juce::Colours::black.withAlpha(0.38f));
            g.fillRoundedRectangle(r, corner);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& b,
                        bool isHighlighted, bool isDown) override
    {
        auto theme = ThemeData::getHardwareTheme();
        const bool on = b.getToggleState();

        g.setFont(juce::Font(juce::FontOptions((float) juce::jmin(13, b.getHeight() - 8),
                                               juce::Font::bold)));
        juce::Colour c = on            ? juce::Colours::white
                       : isHighlighted ? theme.textLabel.brighter(0.25f)
                                       : theme.textLabel.withAlpha(0.82f);
        if (isDown) c = c.darker(0.2f);
        if (!b.isEnabled()) c = c.withAlpha(0.4f);
        g.setColour(c);
        g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }
};

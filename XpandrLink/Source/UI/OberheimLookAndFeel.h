/*
  OberheimLookAndFeel.h
  Version: 18.2 (Fixed ThemeData Members)
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"

class OberheimLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OberheimLookAndFeel()
    {
        setTheme(ThemeData::getHardwareTheme()); // Default
    }

    void setTheme(const ThemeData& newTheme)
    {
        currentTheme = newTheme;
        
        // Update standard JUCE colours
        setColour(juce::TextButton::buttonColourId, currentTheme.buttonNormal);
        setColour(juce::TextButton::textColourOffId, currentTheme.textLabel); // Use textLabel for buttons
        setColour(juce::TextButton::textColourOnId, currentTheme.textValue);  // Brighter for active
        
        setColour(juce::Label::textColourId, currentTheme.textLabel);
        setColour(juce::Slider::textBoxTextColourId, currentTheme.textValue);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    ThemeData& getTheme() { return currentTheme; }

    // --- ROTARY KNOB DRAWING ---
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto center = bounds.getCentre();

        // Draw Active Arc (Value) — thin cyan arc, min→current, no background track
        if (slider.isEnabled())
        {
            juce::Path valueArc;
            valueArc.addCentredArc(center.x, center.y, radius, radius, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(currentTheme.knobIndicatorActive);
            g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Knob Body
        auto knobRadius = radius * 0.7f;
        g.setColour(currentTheme.knobBody);
        g.fillEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        
        // 4. Knob Outline
        g.setColour(currentTheme.groupOutline.withAlpha(0.5f));
        g.drawEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        // 5. Pointer Line
        juce::Path p;
        p.addRectangle(-2.0f, -knobRadius + 4.0f, 4.0f, 6.0f);
        p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));
        g.setColour(currentTheme.knobPointer);
        g.fillPath(p);
    }
    
    // --- TEXT BUTTON DRAWING ---
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, [[maybe_unused]] const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        juce::Colour baseColour;
        
        // Logic for Toggles (Active State)
        if (button.getToggleState()) {
             baseColour = currentTheme.buttonActive; // Green/Lit
        } else {
             baseColour = currentTheme.buttonNormal; // Dark
        }
        
        if (shouldDrawButtonAsDown) baseColour = baseColour.brighter(0.2f);
        else if (shouldDrawButtonAsHighlighted) baseColour = baseColour.brighter(0.1f);
        
        g.setColour(baseColour);
        g.fillRect(bounds);
        
        // Tech Border
        g.setColour(currentTheme.groupOutline);
        g.drawRect(bounds, 1.0f);
        
        // Sci-Fi Corners
        float len = 3.0f;
        g.setColour(currentTheme.textLabel.withAlpha(0.5f));
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX() + len, bounds.getY(), 1.0f);
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX(), bounds.getY() + len, 1.0f);
        g.drawLine(bounds.getRight() - len, bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 1.0f);
        g.drawLine(bounds.getRight(), bounds.getBottom() - len, bounds.getRight(), bounds.getBottom(), 1.0f);
    }

    // --- TAB BAR ---
    void drawTabbedButtonBarBackground(juce::TabbedButtonBar& bar, juce::Graphics& g) override
    {
        g.fillAll(currentTheme.mainBackground);
        // Bottom edge matches the groupOutline border used on all module panels
        g.setColour(currentTheme.groupOutline);
        g.drawHorizontalLine(bar.getHeight() - 1, 0.0f, (float)bar.getWidth());
    }

    void drawTabButton(juce::TabBarButton& button, juce::Graphics& g,
                       bool isMouseOver, bool /*isMouseDown*/) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        bool active = button.isFrontTab();

        // Active tab uses the same headerBand as module group headers
        juce::Colour bg = active ? currentTheme.headerBand
                                 : (isMouseOver ? currentTheme.buttonNormal.brighter(0.15f)
                                                : currentTheme.buttonNormal);
        g.setColour(bg);
        g.fillRect(bounds);

        // Full outline on active tab, dim on inactive
        g.setColour(active ? currentTheme.groupOutline
                           : currentTheme.groupOutline.withAlpha(0.35f));
        g.drawRect(bounds, 1.0f);

        // Active tab: erase the bottom edge so it visually blends into the content area
        if (active)
        {
            g.setColour(currentTheme.headerBand);
            g.drawHorizontalLine((int)bounds.getBottom(),
                                 bounds.getX() + 1.0f, bounds.getRight() - 1.0f);
        }

        drawTabButtonText(button, g, isMouseOver, false);
    }

    void drawTabButtonText(juce::TabBarButton& button, juce::Graphics& g,
                           bool isMouseOver, bool /*isMouseDown*/) override
    {
        bool active = button.isFrontTab();
        g.setColour(active ? currentTheme.textHeader
                           : (isMouseOver ? currentTheme.textLabel.brighter(0.2f)
                                          : currentTheme.textLabel));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred, false);
    }

private:
    ThemeData currentTheme;
};
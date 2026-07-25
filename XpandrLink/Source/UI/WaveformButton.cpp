#include "WaveformButton.h"
#include "HardwareComponents.h"
#include "ThemeData.h"

WaveformButton* WaveformButton::makeWlistButton(const juce::String& label)
{
    auto* b = new WaveformButton(label);
    b->useWlistStyle = true;
    b->getProperties().set("isLed", true);
    return b;
}

WaveformButton* WaveformButton::makeP2Button(const juce::String& label)
{
    auto* b = new WaveformButton(label);
    b->useP2Style = true;
    return b;
}


void WaveformButton::paintButton(juce::Graphics& g, bool, bool)
{
    auto theme = ThemeData::getHardwareTheme();
    auto bounds = getLocalBounds().toFloat();

    if (useP2Style)
    {
        auto lbl = getName().toUpperCase();
        // "alwaysActive" radios (Single/Multi) always show a selection — render lit.
        bool always = getProperties().contains("alwaysActive");
        bool on = (always || getToggleState()) && isEnabled();
        // LED on the left (same 9px circle as WLIST)
        float ledD = 9.0f;
        float ledX = bounds.getX() + 2.0f;
        float ledY = bounds.getCentreY() - ledD * 0.5f;
        if (on) {
            g.setColour(theme.ledOn);
            g.fillEllipse(ledX, ledY, ledD, ledD);
            g.setColour(theme.ledOn.withAlpha(0.28f));
            g.fillEllipse(ledX - 2.0f, ledY - 2.0f, ledD + 4.0f, ledD + 4.0f);
        } else {
            g.setColour(theme.ledOff);
            g.fillEllipse(ledX, ledY, ledD, ledD);
        }
        // VFD box offset right of LED
        auto vfdBox = bounds.withTrimmedLeft(ledD + 5.0f).reduced(0, 1);
        g.setColour(theme.vfdBackground);
        g.fillRect(vfdBox);
        g.setColour(theme.vfdBorder);
        g.drawRect(vfdBox, 1.0f);
        // Text and underscore in separate bands (prevents glyph overlap: FREE→EBEE etc.)
        auto textArea  = vfdBox.withTrimmedBottom(vfdBox.getHeight() * 0.34f);
        g.setColour(on ? theme.textValue : theme.vfdDim);
        float fs = 11.0f;
        auto fnt = ThemeData::getVfdFont(fs);
        while (fs > 7.5f && fontStringWidth(fnt, lbl) > (float)textArea.getWidth()) {
            fs -= 0.5f;
            fnt = ThemeData::getVfdFont(fs);
        }
        g.setFont(fnt);
        g.drawText(lbl, textArea.toNearestInt(), juce::Justification::centredBottom);
        if (on) {
            auto underArea = vfdBox.withTrimmedTop(vfdBox.getHeight() * 0.60f)
                                   .withTrimmedBottom(2.0f);
            g.setColour(theme.textValue);
            g.drawText(juce::String::repeatedString("_", lbl.length()),
                       underArea.toNearestInt(), juce::Justification::centredBottom);
        }
        return;
    }

    // WLIST: 9×9 LED + VFD box behind text (11px DSEG14) + underscore when active
    float ledD = 9.0f;
    float ledX = bounds.getX() + 2.0f;
    float ledY = bounds.getCentreY() - ledD * 0.5f;
    if (getToggleState()) {
        g.setColour(theme.ledOn);
        g.fillEllipse(ledX, ledY, ledD, ledD);
        g.setColour(theme.ledOn.withAlpha(0.28f));
        g.fillEllipse(ledX - 2.0f, ledY - 2.0f, ledD + 4.0f, ledD + 4.0f);
    } else {
        g.setColour(theme.ledOff);
        g.fillEllipse(ledX, ledY, ledD, ledD);
    }
    // VFD background box behind text
    auto vfdR = bounds.withTrimmedLeft(ledD + 5.0f).reduced(0, 1);
    g.setColour(theme.vfdBackground);
    g.fillRect(vfdR);
    g.setColour(theme.vfdBorder);
    g.drawRect(vfdR, 1.0f);

    bool alwaysActive = getProperties().contains("alwaysActive");
    bool on = alwaysActive || getToggleState();
    g.setColour(on ? theme.textValue : theme.vfdDim);
    auto textArea = vfdR.withTrimmedBottom(vfdR.getHeight() * 0.34f);
    float fs = 11.0f;
    auto fnt = ThemeData::getVfdFont(fs);
    while (fs > 7.5f && fontStringWidth(fnt, getName()) > (float)textArea.getWidth()) {
        fs -= 0.5f;
        fnt = ThemeData::getVfdFont(fs);
    }
    g.setFont(fnt);
    g.drawText(getName(), textArea.toNearestInt(), juce::Justification::centredBottom);
    if (on) {
        // Trim 2px from VFD bottom so the DSEG14 "_" bar renders at y≈22-23,
        // clearly inside the VFD box rather than at the border (y=25).
        auto underArea = vfdR.withTrimmedTop(vfdR.getHeight() * 0.60f)
                             .withTrimmedBottom(2.0f);
        g.setColour(theme.textValue);
        g.setFont(ThemeData::getVfdFont(11.0f));
        g.drawText(juce::String::repeatedString("_", getName().length()),
                   underArea.toNearestInt(), juce::Justification::centredBottom);
    }

    if (!isEnabled())
    {
        g.setColour(juce::Colour(0x88000000));
        g.fillRect(getLocalBounds());
    }
}

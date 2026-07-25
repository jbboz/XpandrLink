#include "HardwareComboBoxLookAndFeel.h"
#include "HardwareComponents.h"
#include "ThemeData.h"

void HardwareComboBoxLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                               int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                               juce::ComboBox& box)
{
    auto theme = ThemeData::getHardwareTheme();
    juce::Rectangle<float> bounds(0, 0, (float)width, (float)height);

    int pid = (int)box.getProperties().getWithDefault("paramID", -1);
    bool isValidDest_cb = modLogic != nullptr && modLogic->isValidModDestination(pid);
    bool isRoutingTarget = isValidDest_cb && (modLogic->isRouting()
                         || modLogic->isActivelyRouted(pid));

    // Label above, VFD box below.
    auto labelH = juce::jmin(14.0f, height * 0.46f);
    {
        juce::Font lf(juce::FontOptions(11.0f));
        auto lbl = box.getName().toUpperCase();
        const float ledD = 6.0f, ledTextGap = 4.0f;
        float textW = fontStringWidth(lf, lbl);
        float groupW = ledD + ledTextGap + textW;
        float groupX = juce::jmax(0.0f, (width - groupW) / 2.0f);
        float ledY = labelH / 2.0f - ledD / 2.0f;

        bool isModDest = isValidDest_cb;
        float textOffX = groupX + ledD + ledTextGap;
        if (isRoutingTarget) {
            g.setColour(juce::Colour(0xff00fc2e));
            g.fillEllipse(groupX, ledY, ledD, ledD);
            g.setColour(juce::Colour(0xff00fc2e).withAlpha(0.08f));
            g.fillEllipse(groupX - 3.0f, ledY - 3.0f, ledD + 6.0f, ledD + 6.0f);
            g.setColour(juce::Colour(0xff00fc2e).withAlpha(0.85f));
        } else if (isModDest) {
            g.setColour(juce::Colour(0xff002a0c));
            g.fillEllipse(groupX, ledY, ledD, ledD);
            g.setColour(theme.textLabel);
        } else {
            textOffX = groupX;
            g.setColour(theme.textLabel);
        }
        g.setFont(lf);
        g.drawText(lbl, juce::Rectangle<float>(textOffX, 0.0f, (float)width - textOffX, labelH).toNearestInt(),
                   juce::Justification::centredLeft);
    }

    auto vfdBounds = bounds.withTrimmedTop(labelH).reduced(2, 0);
    g.setColour(theme.vfdBackground);
    g.fillRect(vfdBounds);
    g.setColour(theme.groupOutline);
    g.drawRect(vfdBounds, 1.0f);

    {
        auto vfdInner = vfdBounds.reduced(2, 1).toNearestInt();
        auto valStr   = box.getText();
        auto vfdFont  = ThemeData::getVfdFont(11.0f);
        g.setFont(vfdFont);
        g.setColour(theme.vfdGhost);
        g.drawText(juce::String::repeatedString("~", valStr.length()),
                   vfdInner, juce::Justification::centredRight);
        g.setColour(theme.textValue);
        g.drawText(valStr, vfdInner, juce::Justification::centredRight);
    }

    g.setColour(theme.textLabel);
    juce::Path tri;
    float arrowX = vfdBounds.getRight() - 8;
    float arrowY = vfdBounds.getCentreY();
    tri.addTriangle(arrowX, arrowY - 2, arrowX + 4, arrowY - 2, arrowX + 2, arrowY + 2);
    g.fillPath(tri);
}

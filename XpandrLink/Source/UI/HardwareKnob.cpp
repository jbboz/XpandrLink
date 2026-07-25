#include "HardwareKnob.h"

HardwareKnob::HardwareKnob(const juce::String& labelText, ModAssignmentLogic* modLogic)
    : name(labelText), modAssignmentLogic(modLogic)
{
    setLookAndFeel(&noTextLF);
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity(100);
}

HardwareKnob::~HardwareKnob() { setLookAndFeel(nullptr); }

void HardwareKnob::mouseDown(const juce::MouseEvent& e)
{
    if (modAssignmentLogic && modAssignmentLogic->isRouting())
    {
        if (modAssignmentLogic->isValidModDestination(paramID))
        {
            if (onAssignmentCallback) onAssignmentCallback();
            if (auto* top = getTopLevelComponent()) top->repaint();
        }
        return;
    }
    if (modAssignmentLogic) modAssignmentLogic->notifyDestinationFocused(paramID);
    juce::Slider::mouseDown(e);
}

void HardwareKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    if (modAssignmentLogic && modAssignmentLogic->isRouting()) return;

    // Label (14px) sits above VFD (22px)
    auto vfdInScreen = localAreaToGlobal(getLocalBounds().withTrimmedTop(14).removeFromTop(22).reduced(2, 0));
    auto* content = new NumericEntry((int)getValue(), (int)getMinimum(), (int)getMaximum(),
                                     [this](int v) { setValue((double)v, juce::sendNotificationAsync); });
    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(content),
                                           vfdInScreen, nullptr);
}

void HardwareKnob::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();
    auto bounds = getLocalBounds().toFloat();

    bool isValidDest_hk = modAssignmentLogic != nullptr && modAssignmentLogic->isValidModDestination(paramID);
    bool isRoutingTarget = isValidDest_hk && (modAssignmentLogic->isRouting()
                         || modAssignmentLogic->isActivelyRouted(paramID));

    // Label (14px at top) — LED+text centered as a group above VFD
    auto labelArea = bounds.removeFromTop(14);
    auto labelFont = juce::Font(juce::FontOptions(11.0f));
    auto labelText = name.toUpperCase();
    {
        const float ledD = 6.0f;
        const float ledTextGap = 4.0f;
        float textW = fontStringWidth(labelFont, labelText);
        float groupW = ledD + ledTextGap + textW;
        // Center the LED+text group within the label area
        float groupX = labelArea.getX() + (labelArea.getWidth() - groupW) / 2.0f;
        groupX = juce::jmax(labelArea.getX() + 1.0f, groupX);
        float ledX = groupX;
        float ledY = labelArea.getCentreY() - ledD / 2.0f;

        bool isModDest = isValidDest_hk;
        float textX = groupX + ledD + ledTextGap;
        if (isRoutingTarget)
        {
            g.setColour(juce::Colour(0xff00fc2e));
            g.fillEllipse(ledX, ledY, ledD, ledD);
            g.setColour(juce::Colour(0xff00fc2e).withAlpha(0.08f));
            g.fillEllipse(ledX - 3.0f, ledY - 3.0f, ledD + 6.0f, ledD + 6.0f);
            g.setColour(juce::Colour(0xff00fc2e).withAlpha(0.85f));
        }
        else if (isModDest)
        {
            g.setColour(juce::Colour(0xff002a0c));
            g.fillEllipse(ledX, ledY, ledD, ledD);
            g.setColour(theme.textLabel);
        }
        else
        {
            textX = groupX; // no LED — text starts at the group origin
            g.setColour(theme.textLabel);
        }
        g.setFont(labelFont);
        float textY = (float)labelArea.getY();
        g.drawText(labelText,
                   juce::Rectangle<float>(textX, textY, textW + 2.0f, (float)labelArea.getHeight()).toNearestInt(),
                   juce::Justification::centredLeft);
    }

    // VFD box (22px) with green border per design spec
    auto vfdArea = bounds.removeFromTop(22).reduced(2, 0);
    g.setColour(theme.vfdBackground);
    g.fillRect(vfdArea);
    g.setColour(theme.vfdBorder);  // rgb(28,64,40) greenish border
    g.drawRect(vfdArea, 1.0f);

    const bool isBipolar = (getMinimum() < 0.0);
    const double val     = getValue();

    // VFD display — bipolar shows +N (blue) or -N (red), unipolar shows N (green)
    {
        auto vfdInner = vfdArea.reduced(3, 1).toNearestInt();
        juce::String valStr;
        juce::Colour litColor = theme.textValue;
        if (isBipolar) {
            int iv = (int)val;
            valStr = (iv > 0) ? "+" + juce::String(iv) : juce::String(iv);
            litColor = (iv > 0) ? juce::Colour(0xff6478ff)   // blue for positive
                     : (iv < 0) ? juce::Colour(0xffff5050)   // red for negative
                                : theme.textValue;
        } else {
            valStr = juce::String((int)val);
        }
        g.setFont(ThemeData::getVfdFont(13.0f));
        g.setColour(theme.vfdGhost);
        g.drawText(juce::String::repeatedString("~", juce::jmax(2, valStr.length())),
                   vfdInner, juce::Justification::centredRight);
        g.setColour(litColor);
        g.drawText(valStr, vfdInner, juce::Justification::centredRight);
    }

    auto rotaryParams = getRotaryParameters();
    auto  center  = bounds.getCentre();
    float availR  = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    float knobR   = availR * 0.70f;
    float arcR    = availR * 0.90f;

    // Knob body
    g.setColour(theme.knobBody);
    g.fillEllipse(center.x - knobR, center.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour(juce::Colour(0xff000000));
    g.drawEllipse(center.x - knobR, center.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    if (isBipolar)
    {
        // Bipolar: arc from 12-o'clock center toward current value
        float centerAngle = (rotaryParams.startAngleRadians + rotaryParams.endAngleRadians) / 2.0f;
        float halfArc     = (rotaryParams.endAngleRadians - rotaryParams.startAngleRadians) / 2.0f;
        float bipRange    = (float)juce::jmax(getMaximum(), -getMinimum());
        float norm        = (bipRange > 0.0f) ? (float)(val / bipRange) : 0.0f;
        norm = juce::jlimit(-1.0f, 1.0f, norm);
        float endAngle    = centerAngle + norm * halfArc;

        // Small gray center-reference dot at 12 o'clock
        float cdx = center.x + arcR * std::sin(centerAngle - (float)juce::MathConstants<double>::halfPi + (float)juce::MathConstants<double>::halfPi);
        float cdy = center.y - arcR;  // 12 o'clock
        g.setColour(juce::Colour(0xff555555));
        g.fillEllipse(cdx - 1.5f, cdy - 1.5f, 3.0f, 3.0f);

        // Colored arc from center to end
        if (std::abs(norm) > 0.01f) {
            juce::Path arcPath;
            arcPath.addCentredArc(center.x, center.y, arcR, arcR, 0.0f,
                                  centerAngle, endAngle, true);
            juce::Colour arcCol = (norm > 0) ? juce::Colour(0xff6478ff) : juce::Colour(0xffff5050);
            g.setColour(arcCol);
            g.strokePath(arcPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        // Pointer at end angle
        g.setColour(theme.knobPointer);
        juce::Path p;
        p.addRectangle(-2.0f, -knobR + 2.0f, 4.0f, 6.0f);
        p.applyTransform(juce::AffineTransform::rotation(endAngle).translated(center.x, center.y));
        g.fillPath(p);
    }
    else
    {
        // Unipolar: arc from start angle to current value (existing behavior)
        float norm  = (getMaximum() > getMinimum())
                    ? (float)((val - getMinimum()) / (getMaximum() - getMinimum()))
                    : 0.0f;
        float angle = rotaryParams.startAngleRadians
                    + norm * (rotaryParams.endAngleRadians - rotaryParams.startAngleRadians);

        if (getMaximum() > getMinimum()) {
            juce::Path arcPath;
            arcPath.addCentredArc(center.x, center.y, arcR, arcR, 0.0f,
                                  rotaryParams.startAngleRadians, angle, true);
            g.setColour(theme.knobIndicatorActive);
            g.strokePath(arcPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        g.setColour(theme.knobPointer);
        juce::Path p;
        p.addRectangle(-2.0f, -knobR + 2.0f, 4.0f, 6.0f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(center.x, center.y));
        g.fillPath(p);
    }
}

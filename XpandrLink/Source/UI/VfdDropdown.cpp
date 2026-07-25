#include "VfdDropdown.h"
#include "HardwareComponents.h"

VfdDropdown::VfdDropdown(const juce::String& labelText, ModAssignmentLogic* modLogic)
    : name(labelText), modAssignmentLogic(modLogic)
{
    setWantsKeyboardFocus(true);
}

void VfdDropdown::setItems(const juce::StringArray& newItems)
{
    items = newItems;
    selectedIdx = juce::jlimit(0, juce::jmax(0, items.size() - 1), selectedIdx);
    repaint();
}

void VfdDropdown::setSelectedIndex(int idx)
{
    selectedIdx = juce::jlimit(0, juce::jmax(0, items.size() - 1), idx);
    repaint();
}

void VfdDropdown::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();
    auto bounds = getLocalBounds().toFloat();

    bool isValidDest_vd = modAssignmentLogic != nullptr && modAssignmentLogic->isValidModDestination(paramID);
    bool isRoutingTarget = isValidDest_vd && (modAssignmentLogic->isRouting()
                         || modAssignmentLogic->isActivelyRouted(paramID));

    // Empty-name dropdowns (e.g. Page-2 LFO Src) render as a pure VFD box that
    // matches the size of P2 buttons — no reserved label strip.
    const bool showLabel = name.isNotEmpty();

    // Label (14px) at top — LED+text centered as a group
    auto labelArea = showLabel ? bounds.removeFromTop(14) : juce::Rectangle<float>();
    auto labelFont = juce::Font(juce::FontOptions(11.0f));
    auto labelText = name.toUpperCase();
    if (showLabel)
    {
        const float ledD = 6.0f;
        const float ledTextGap = 4.0f;
        float textW = fontStringWidth(labelFont, labelText);
        float groupW = ledD + ledTextGap + textW;
        float groupX = labelArea.getX() + (labelArea.getWidth() - groupW) / 2.0f;
        groupX = juce::jmax(labelArea.getX() + 1.0f, groupX);
        float ledX = groupX;
        float ledY = labelArea.getCentreY() - ledD / 2.0f;

        bool isModDest2 = isValidDest_vd;
        float textOffX2 = groupX + ledD + ledTextGap;
        if (isRoutingTarget)
        {
            g.setColour(juce::Colour(0xff00fc2e));
            g.fillEllipse(ledX, ledY, ledD, ledD);
            g.setColour(juce::Colour(0xff00fc2e).withAlpha(0.08f));
            g.fillEllipse(ledX - 3.0f, ledY - 3.0f, ledD + 6.0f, ledD + 6.0f);
            g.setColour(juce::Colour(0xff00fc2e).withAlpha(0.85f));
        }
        else if (isModDest2)
        {
            g.setColour(juce::Colour(0xff002a0c));
            g.fillEllipse(ledX, ledY, ledD, ledD);
            g.setColour(theme.textLabel);
        }
        else
        {
            textOffX2 = groupX;
            g.setColour(theme.textLabel);
        }
        g.setFont(labelFont);
        g.drawText(labelText,
                   juce::Rectangle<float>(textOffX2, (float)labelArea.getY(),
                                          textW + 2.0f, (float)labelArea.getHeight()).toNearestInt(),
                   juce::Justification::centredLeft);
    }

    // VFD box fills the remaining height (capped at 26 when a label is shown so
    // labeled dropdowns match the P2 button box height; empty-name dropdowns fill
    // the whole control, matching P2 buttons exactly).
    float boxH = showLabel ? juce::jmin(bounds.getHeight(), 26.0f) : bounds.getHeight();
    auto vfdArea = bounds.removeFromTop(boxH).reduced(2, 1);
    g.setColour(theme.vfdBackground);
    g.fillRect(vfdArea);
    g.setColour(theme.vfdBorder);
    g.drawRect(vfdArea, 1.0f);

    // Current selection in DSEG14 — use the smaller P2 font for the short P2 boxes so
    // dropdowns read at the same text height as the P2 buttons (and don't look taller);
    // the taller main-editor dropdown boxes keep the larger 13px font.
    if (selectedIdx >= 0 && selectedIdx < items.size())
    {
        auto inner  = vfdArea.reduced(3, 1).toNearestInt();
        auto valStr = items[selectedIdx];
        g.setFont(ThemeData::getVfdFont(vfdArea.getHeight() < 30.0f ? 11.0f : 13.0f));
        g.setColour(theme.vfdGhost);
        g.drawText(juce::String::repeatedString("~", juce::jmax(2, valStr.length())), inner, juce::Justification::centredLeft);
        g.setColour(theme.textValue);
        g.drawText(valStr, inner, juce::Justification::centredLeft);
    }

    if (!isEnabled())
    {
        g.setColour(juce::Colour(0x88000000));
        // Dim only the VFD value box, not the label — the parameter title stays bright
        // and consistent with enabled controls; only the value reads as "off". (Also
        // avoids the full-bounds dark rectangle below a short box in compact layout.)
        auto dimArea = getLocalBounds();
        if (showLabel) dimArea.removeFromTop(14);
        g.fillRect(dimArea.removeFromTop((int) boxH));
    }
}

void VfdDropdown::mouseDown(const juce::MouseEvent&)
{
    if (!isEnabled()) return;

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

    // Screen rect of the VFD box (label 14px + VFD 22px)
    auto vfdScreen = localAreaToGlobal(
        getLocalBounds().withTrimmedTop(14).removeFromTop(22).reduced(2, 0));

    juce::Component::SafePointer<VfdDropdown> safe(this);
    VfdPopupList::show(items, selectedIdx, vfdScreen, [safe](int idx) {
        if (!safe) return;
        safe->selectedIdx = idx;
        safe->repaint();
        if (safe->onChange) safe->onChange(idx);
    });
}

bool VfdDropdown::keyPressed(const juce::KeyPress& key)
{
    if (!isEnabled() || items.isEmpty()) return false;
    int newIdx = selectedIdx;
    if      (key == juce::KeyPress::upKey)   newIdx = juce::jmax(0, selectedIdx - 1);
    else if (key == juce::KeyPress::downKey) newIdx = juce::jmin(items.size() - 1, selectedIdx + 1);
    else return false;

    if (newIdx != selectedIdx) {
        selectedIdx = newIdx;
        repaint();
        if (onChange) onChange(selectedIdx);
    }
    return true;
}

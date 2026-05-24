/*
  HardwareComponents.cpp
  Implementations split out from HardwareComponents.h (TASK-13a).
*/
#include "HardwareComponents.h"

static float fontStringWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// =============================================================================
// HardwareComboBoxLookAndFeel
// =============================================================================

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

// =============================================================================
// HardwareKnob
// =============================================================================

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

// =============================================================================
// HardwareMenu
// =============================================================================

HardwareMenu::HardwareMenu(const juce::String& name, ModAssignmentLogic* modLogic)
    : modAssignmentLogic(modLogic)
{
    menuLF.modLogic = modLogic;
    setName(name);
    setLookAndFeel(&menuLF);
    setJustificationType(juce::Justification::centred);
    setTextWhenNothingSelected("Select");
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
}

HardwareMenu::~HardwareMenu() { setLookAndFeel(nullptr); }

void HardwareMenu::mouseDown(const juce::MouseEvent& e)
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
    juce::ComboBox::mouseDown(e);
}

// =============================================================================
// VfdPopupList
// =============================================================================

VfdPopupList::VfdPopupList(const juce::StringArray& items, int sel,
                           juce::Rectangle<int> below,
                           std::function<void(int)> cb)
    : items_(items), selectedIdx_(sel), callback_(std::move(cb))
{
    const int itemH   = 18;
    const int padding = 4;
    int visCount      = juce::jmin(13, items_.size());
    int listH         = visCount * itemH + padding * 2;
    // Scroll so selected item is centred in the visible range
    scrollOffset_ = juce::jlimit(0, juce::jmax(0, items_.size() - visCount),
                                 juce::jmax(0, sel - visCount / 2));
    setBounds(below.getX(), below.getBottom() + 2,
              juce::jmax(below.getWidth() + 40, 160), listH);
}

void VfdPopupList::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();
    g.setColour(theme.vfdBackground);
    g.fillRect(getLocalBounds());
    g.setColour(theme.vfdBorder);
    g.drawRect(getLocalBounds().toFloat(), 1.0f);
    g.setColour(juce::Colour(0x44000000));
    g.drawRect(getLocalBounds().expanded(1).toFloat(), 1.0f);

    const int itemH  = 18;
    const int scrollW = 4;
    const int padding = 4;
    int total    = items_.size();
    auto listArea = getLocalBounds().reduced(1).withTrimmedRight(total > visibleCount() ? scrollW + 1 : 0);

    // Scrollbar thumb
    int vis = visibleCount();
    if (total > vis)
    {
        float ratio = (float)vis / (float)total;
        float posF  = (float)scrollOffset_ / (float)total;
        auto sb = juce::Rectangle<float>(
            (float)(getWidth() - scrollW - 1), (float)padding,
            (float)scrollW, ratio * (float)(getHeight() - padding * 2));
        g.setColour(theme.textLabel.withAlpha(0.35f));
        g.fillRect(sb);
    }

    g.setFont(ThemeData::getVfdFont(11.0f));
    for (int i = scrollOffset_; i < total; ++i)
    {
        int yOff = padding + (i - scrollOffset_) * itemH;
        if (yOff + itemH > getHeight()) break;
        auto row = juce::Rectangle<int>(listArea.getX(), yOff, listArea.getWidth(), itemH);
        if (i == selectedIdx_) {
            g.setColour(theme.textValue.withAlpha(0.12f));
            g.fillRect(row);
        } else if (i == hoverIdx_) {
            g.setColour(theme.textValue.withAlpha(0.05f));
            g.fillRect(row);
        }
        g.setColour(i == selectedIdx_ ? theme.textValue
                  : i == hoverIdx_    ? theme.textValue.withAlpha(0.65f)
                                     : theme.vfdDim);
        g.drawText(items_[i], row.reduced(6, 0), juce::Justification::centredLeft);
    }
}

void VfdPopupList::mouseMove(const juce::MouseEvent& e)
{
    int idx = hitItem(e.y);
    if (idx != hoverIdx_) { hoverIdx_ = idx; repaint(); }
}

void VfdPopupList::mouseExit(const juce::MouseEvent&) { hoverIdx_ = -1; repaint(); }

void VfdPopupList::mouseDown(const juce::MouseEvent& e)
{
    auto local = e.getEventRelativeTo(this).getPosition();
    if (!getLocalBounds().contains(local)) { dismiss(); return; }
    int idx = hitItem(local.y);
    if (idx >= 0 && idx < items_.size()) {
        selectedIdx_ = idx;
        if (callback_) callback_(idx);
    }
    dismiss();
}

void VfdPopupList::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    int delta = (w.deltaY > 0.0f) ? -1 : 1;
    scrollOffset_ = juce::jlimit(0, juce::jmax(0, items_.size() - visibleCount()),
                                 scrollOffset_ + delta);
    repaint();
}

void VfdPopupList::timerCallback()
{
    if (dismissed_) return;
    if (!juce::Process::isForegroundProcess())
        dismiss();
}

int VfdPopupList::hitItem(int y) const
{
    int idx = (y - 4) / 18 + scrollOffset_;
    return (idx >= scrollOffset_ && idx < scrollOffset_ + visibleCount()
            && idx >= 0 && idx < items_.size()) ? idx : -1;
}

void VfdPopupList::dismiss()
{
    if (dismissed_) return;
    dismissed_ = true;
    stopTimer();
    if (isCurrentlyModal()) exitModalState(0);
    setVisible(false);
    removeFromDesktop(); // severs OS callbacks so focusLost can't fire after delete
    juce::MessageManager::callAsync([this] { delete this; });
}

void VfdPopupList::show(const juce::StringArray& items, int sel,
                        juce::Rectangle<int> below, std::function<void(int)> cb)
{
    auto* p = new VfdPopupList(items, sel, below, std::move(cb));
    p->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    p->setAlwaysOnTop(true);
    p->setVisible(true);
    p->toFront(true);
    // Non-blocking modal state so a click anywhere outside the popup routes to
    // inputAttemptWhenModal() → dismiss(). Without this the popup only closed on
    // selecting an item, since outside clicks go to the main window, not the popup.
    p->enterModalState(false);
    p->startTimer(100); // hide when app loses foreground
}

// =============================================================================
// VfdDropdown
// =============================================================================

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

// =============================================================================
// WaveformButton
// =============================================================================

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

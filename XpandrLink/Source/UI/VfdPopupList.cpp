#include "VfdPopupList.h"
#include "HardwareComponents.h"

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

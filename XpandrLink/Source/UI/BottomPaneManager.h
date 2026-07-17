#pragma once
#include <JuceHeader.h>
#include <functional>
#include <vector>

// Coordinates the bottom-pane toggle system: state machine, button sync, and layout.
// EditorTabComponent retains ownership of all buttons and panels; this class holds
// non-owning pointers. Register panes in left-to-right button order via addPane().
class BottomPaneManager
{
public:
    static constexpr int kPaneHeight = 180;

    // Fires after a pane toggle so the owner can re-lay out (e.g. call resized()).
    std::function<void()> onLayoutChanged;

    // Register a pane. Call once per pane during EditorTabComponent construction,
    // in left-to-right button order. onOpen fires when this pane transitions closed→open.
    void addPane(juce::TextButton*     btn,
                 juce::Component*      panel,
                 std::function<void()> onOpen = {})
    {
        jassert(btn != nullptr && panel != nullptr);
        const int index = static_cast<int>(entries_.size());
        entries_.push_back({ btn, panel, std::move(onOpen), {} });
        btn->onClick = [this, index] {
            toggle(index);
            if (onLayoutChanged) onLayoutChanged();
        };
    }

    // Register a momentary action button (no pane). Fires onClick; never toggles/lits.
    void addAction(juce::TextButton* btn, std::function<void()> onClick)
    {
        jassert(btn != nullptr);
        const int index = static_cast<int>(entries_.size());
        entries_.push_back({ btn, nullptr, {}, std::move(onClick) });
        btn->onClick = [this, index] {
            if (entries_[index].onClick) entries_[index].onClick();
        };
    }

    // Toggle pane at index. Same index again = close. Different index = switch.
    // Caller must call resized() afterwards to update layout.
    void toggle(int index)
    {
        if (index < 0 || index >= static_cast<int>(entries_.size()))
            return;
        if (entries_[index].panel == nullptr) return;   // action entry — not toggleable
        if (openIndex_ == index)
        {
            openIndex_ = -1;
        }
        else
        {
            openIndex_ = index;
            if (entries_[index].onOpen)
                entries_[index].onOpen();
        }
        syncButtonStates();
    }

    // Position all registered buttons flush-right inside navBar.
    void layoutNavButtons(juce::Rectangle<int> navBar, int btnW, int btnH, int margin)
    {
        if (entries_.empty()) return;
        int count   = static_cast<int>(entries_.size());
        int totalW  = count * btnW + (count - 1) * margin;
        int x       = navBar.getRight() - totalW - 12;
        int cy      = navBar.getCentreY();
        for (auto& e : entries_)
        {
            e.btn->setBounds(x, cy - btnH / 2, btnW, btnH);
            x += btnW + margin;
        }
    }

    // Set visibility and bounds of the active panel; hide all others.
    // Call only when isOpen() is true.
    void layoutPane(juce::Rectangle<int> paneArea)
    {
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        {
            if (entries_[i].panel == nullptr) continue;
            bool show = (i == openIndex_);
            entries_[i].panel->setVisible(show);
            if (show)
                entries_[i].panel->setBounds(paneArea);
        }
    }

    // Hide all panels. Call when isOpen() is false.
    void hideAll()
    {
        for (auto& e : entries_)
            if (e.panel != nullptr)
                e.panel->setVisible(false);
    }

    bool isOpen()    const { return openIndex_ >= 0; }
    int  openIndex() const { return openIndex_; }

private:
    struct Entry
    {
        juce::TextButton*     btn;
        juce::Component*      panel;   // nullptr for momentary action buttons
        std::function<void()> onOpen;
        std::function<void()> onClick; // set for action entries; unused for panes
    };

    std::vector<Entry> entries_;
    int openIndex_ = -1;

    void syncButtonStates()
    {
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        {
            entries_[i].btn->setToggleState(i == openIndex_, juce::dontSendNotification);
            entries_[i].btn->repaint();
        }
    }
};

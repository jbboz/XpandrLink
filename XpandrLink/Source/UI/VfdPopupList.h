#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"

// --- VFD POPUP LIST — scrollable overlay opened by VfdDropdown and FullModMatrixPanel ---
// Positioned just below 'below' (screen rect of the invoking VFD box).
// Caller must: addToDesktop(windowIsTemporary); setVisible(true); toFront(true).
class VfdPopupList : public juce::Component, private juce::Timer
{
public:
    VfdPopupList(const juce::StringArray& items, int sel,
                 juce::Rectangle<int> below,
                 std::function<void(int)> cb);

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override;
    void inputAttemptWhenModal() override { dismiss(); }
    // focusLost can fire from setVisible(false) on macOS; guard prevents double-free.
    void focusLost(FocusChangeType) override { dismiss(); }
    // Timer: dismiss when the host app loses foreground so the popup doesn't
    // float over other macOS applications.
    void timerCallback() override;

    // Convenience factory: construct, add to desktop, and make visible in one call.
    static void show(const juce::StringArray& items, int sel,
                     juce::Rectangle<int> below, std::function<void(int)> cb);

private:
    juce::StringArray items_;
    int selectedIdx_;
    int hoverIdx_     = -1;
    int scrollOffset_ = 0;
    bool dismissed_   = false;
    std::function<void(int)> callback_;

    int visibleCount() const { return juce::jmax(1, (getHeight() - 8) / 18); }
    int hitItem(int y) const;
    void dismiss();
};

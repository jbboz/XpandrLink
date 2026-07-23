/*
  TimbreSpacePanel.h
  Bottom-pane UI for the Timbre Space (2-D patch-blending map). Drag anywhere in
  the map to hear a live weighted blend of the nearby library patches on the
  hardware. Sends are coalesced behind a 300ms timer (same guard as MorphPanel).
  Undo restores the patch that was active when the pane opened.
*/
#pragma once
#include <JuceHeader.h>
#include <map>
#include <array>
#include <vector>
#include "ThemeData.h"
#include "HardwareButtonLookAndFeel.h"
#include "../Data/TimbreSpaceEngine.h"
#include "../Data/PatchBlender.h"

class TimbreSpacePanel : public juce::Component, private juce::Timer
{
public:
    using ApplyFn = std::function<void(const std::map<int,int>&,
                                       const std::array<uint8_t,60>&,
                                       const juce::String&)>;
    ApplyFn onApply;

    // Fired when the user clicks REFRESH — owner should re-derive the patch
    // list from the current library view and call setPatches() again. Leaves
    // the Undo baseline untouched (only setPatches() changes; setBaseline()
    // is not re-called).
    std::function<void()> onRefreshRequested;

    // Fired when the user clicks ALL or FAV, with the requested new scope.
    // Owner should apply it to the library (PatchBrowserPanel is the single
    // source of truth for this flag), rebuild the patch set, then call
    // setScopeState() to echo the actual resulting state back in.
    std::function<void(bool wantsFavoritesOnly)> onScopeToggled;

    TimbreSpacePanel();
    ~TimbreSpacePanel() override;

    // (Re)build the space from the given patches (called on pane open).
    void setPatches(std::vector<TimbrePatch> patches);

    // Snapshot the patch active at open time, for Undo.
    void setBaseline(std::map<int,int> params, std::array<uint8_t,60> mod, juce::String name);

    // Reflects the given Favorites/All state in the ALL/FAV toggle buttons.
    // Does NOT fire onScopeToggled — this is the echo-from-source setter.
    void setScopeState(bool showFavoritesOnly);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void updateFromPosition(juce::Point<float> local);
    void doUndo();
    juce::Rectangle<int> mapArea() const;

    HardwareButtonLookAndFeel lf_;
    juce::Label      titleLabel_, readoutLabel_, hintLabel_;
    juce::TextButton undoBtn_, refreshBtn_, scopeAllBtn_, scopeFavBtn_;

    TimbreSpaceEngine        engine_;
    std::vector<TimbrePatch> patches_;

    bool                     hasCursor_ = false;
    juce::Point<float>       cursorNorm_ { 0.5f, 0.5f };
    std::vector<WeightedPatch> lastWeights_;

    bool                     hasBaseline_ = false;
    std::map<int,int>        baseParams_;
    std::array<uint8_t,60>   baseMod_{};
    juce::String             baseName_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbreSpacePanel)
};

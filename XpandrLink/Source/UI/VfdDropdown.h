#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"
#include "ModAssignmentLogic.h"
#include "VfdPopupList.h"   // VfdDropdown::mouseDown opens a VfdPopupList

// --- VFD DROPDOWN — shows one selected item, click opens popup VfdScrollList ---
class VfdDropdown : public juce::Component
{
public:
    VfdDropdown(const juce::String& labelText, ModAssignmentLogic* modLogic = nullptr);

    int paramID = -1;
    std::function<void(int)>  onChange;
    std::function<void()>     onAssignmentCallback;

    void setItems(const juce::StringArray& newItems);
    void setSelectedIndex(int idx);

    int getSelectedIndex() const { return selectedIdx; }
    bool hasLabel() const { return name.isNotEmpty(); }
    int  maxItemLength() const { int m = 0; for (auto& s : items) m = juce::jmax(m, s.length()); return m; }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    juce::String        name;
    ModAssignmentLogic* modAssignmentLogic = nullptr;
    juce::StringArray   items;
    int selectedIdx = 0;
};

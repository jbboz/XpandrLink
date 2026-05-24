/*
  GroupTabPanel.h
  Version: 19.0 (Hardware Theme Support)
*/
#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "ModulePanel.h"
#include "OberheimLookAndFeel.h"
#include "ThemeData.h"

class GroupTabPanel : public juce::Component
{
public:
    GroupTabPanel(const juce::String& headerName)
        : groupName(headerName),
          myRadioGroupId(nextRadioGroupId.fetch_add(1, std::memory_order_relaxed))
    {}

    void addTab(const juce::String& tabName, std::unique_ptr<ModulePanel> panel)
    {
        // Hide panel border since the Group handles the outer frame
        panel->setEmbeddedMode(true);
        addChildComponent(panel.get()); 
        
        auto* btn = buttons.add(new juce::TextButton(tabName));
        btn->setClickingTogglesState(true);
        btn->setRadioGroupId(myRadioGroupId);
        
        // Small rectangular buttons in the header
        btn->setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        
        btn->onClick = [this, index = modules.size()] { selectTab(index); };
        
        addAndMakeVisible(btn);
        modules.add(std::move(panel));
        
        if (modules.size() == 1) selectTab(0);
    }

    void selectTab(int index)
    {
        for (int i = 0; i < modules.size(); ++i)
        {
            modules[i]->setVisible(i == index);
            buttons[i]->setToggleState(i == index, juce::dontSendNotification);
        }
        resized();
        if (onTabSelected) onTabSelected(index);
    }

    // Fired when user clicks a tab button. Arg = 0-based tab index.
    std::function<void(int)> onTabSelected;

    void paint(juce::Graphics& g) override
    {
        auto* lf = dynamic_cast<OberheimLookAndFeel*>(&getLookAndFeel());
        auto theme = lf ? lf->getTheme() : ThemeData::getHardwareTheme();
        auto bounds = getLocalBounds().toFloat();
        
        // 1. Tech Border (Cyan)
        g.setColour(theme.groupOutline);
        g.drawRect(bounds, 1.0f);

        // 2. Header Strip (Blue)
        // The header is a solid bar at the top
        auto headerHeight = 22.0f;
        g.setColour(theme.headerBand); 
        g.fillRect(1.0f, 1.0f, bounds.getWidth()-2.0f, headerHeight);
        
        // 3. Group Label — param-label color per DESIGN-v2 §3
        g.setColour(theme.textLabel);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(groupName, 10, 0, 100, (int)headerHeight, juce::Justification::centredLeft);
    }

    void resized() override
    {
        // Trim sides and top only — no bottom trim so the inner ModulePanel reaches the
        // same absolute bottom edge as a sibling direct ModulePanel (same row height).
        // This lets flex-end knob rows align horizontally across all row-1 panels.
        auto area = getLocalBounds()
                        .withTrimmedLeft(1).withTrimmedRight(1).withTrimmedTop(1);
        area.removeFromTop(22); // header consumed by paint()

        // Compute tab button width: fit all buttons into the right side of the header,
        // leaving at least 50px on the left for the group name label.
        int numBtns  = buttons.size();
        int margin   = 2;
        int labelW   = 50;
        int available = getWidth() - labelW - margin;
        int btnW     = numBtns > 0 ? juce::jmin(46, (available - margin * (numBtns - 1)) / numBtns) : 46;
        int btnH     = 18;
        int x        = getWidth() - margin;

        for (int i = buttons.size() - 1; i >= 0; --i)
        {
            x -= btnW;
            buttons[i]->setBounds(x, 2, btnW, btnH);
            x -= margin;
        }

        for (auto* m : modules)
        {
            // Trim sides and top only — no bottom trim (see comment above).
            if (m->isVisible())
                m->setBounds(area.withTrimmedLeft(4).withTrimmedRight(4).withTrimmedTop(4));
        }
    }
    
    // --- Data Forwarding ---
    void forEachPanel(const std::function<void(ModulePanel*)>& cb) { for (auto* m : modules) cb(m); }
    void updateParameter(int paramID, int value) { for (auto* m : modules) m->updateParameter(paramID, value); }
    void setButtonEnabled(int paramID, bool enabled) { for (auto* m : modules) m->setButtonEnabled(paramID, enabled); }
    void setScrollListEnabled(int paramID, bool enabled) { for (auto* m : modules) m->setScrollListEnabled(paramID, enabled); }
    void setParameterChangedCallback(std::function<void(int, int)> cb) { for (auto* m : modules) m->onParameterInteracted = cb; }
    void setPage2ButtonStyle(bool useP2) { for (auto* m : modules) m->setPage2ButtonStyle(useP2); }
    int getParameterValue(int paramID) {
        for (auto* m : modules) {
            int val = m->getParameterValue(paramID);
            if (val != -999) return val;
        }
        return -999;
    }

private:
    juce::String groupName;
    int myRadioGroupId;
    juce::OwnedArray<ModulePanel> modules;
    juce::OwnedArray<juce::TextButton> buttons;

    inline static std::atomic<int> nextRadioGroupId { 1000 };
};
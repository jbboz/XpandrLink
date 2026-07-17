/*
  ModulePanel.h
  Version: 31.0 (Split into .h/.cpp — TASK-13a)
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "ModAssignmentLogic.h"
#include "OberheimLookAndFeel.h"
#include "../Data/XpanderData.h"
#include "HardwareComponents.h"
#include "FilterGraph.h"
#include "EnvelopeGraph.h"
#include "TrackingGraph.h"

class ModulePanel : public juce::Component
{
public:
    ModulePanel(const juce::String& title, const juce::String& groupID, MidiEngine* engine,
                const juce::String& additionalGroup = "", ModAssignmentLogic* modLogic = nullptr);

    void setEmbeddedMode(bool isEmbedded) { isEmbed = isEmbedded; repaint(); }
    void setCompactLayout(bool compact)   { compactLayout = compact; }
    void setKnobTopOffset(int px)         { knobTopOffset = px; }

    void setWlistButtonStyle(bool useWlist);

    // BUG-19: Page-2 style — no LED, VFD font text, underscore active indicator.
    // wlistButtonMode is also enabled so P2 buttons enter the flex (not the LED section).
    void setPage2ButtonStyle(bool useP2);

    void setButtonEnabled(int paramID, bool enabled);
    void setScrollListEnabled(int paramID, bool enabled);

    // Report every owned paramID to the callback so callers can build lookup maps.
    void registerParams(const std::function<void(int, ModulePanel*)>& cb);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateParameter(int paramID, int value);
    int  getParameterValue(int paramID);

    std::function<void(int, int)> onParameterInteracted; // fires after any button click: (paramID, newValue)

private:
    ModAssignmentLogic* modAssignmentLogic = nullptr;
    juce::String moduleTitle;
    juce::String myGroupID;
    bool isEmbed = false;
    bool compactLayout = false;
    int  knobTopOffset = 0;
    bool wlistButtonMode = false;
    bool page2Mode = false;
    std::unique_ptr<FilterGraph>    filterGraph;
    std::unique_ptr<EnvelopeGraph>  envGraph;
    std::unique_ptr<TrackingGraph>  trackGraph;

    juce::OwnedArray<juce::Component>             componentList;
    std::map<int, HardwareKnob*>                  knobMap;
    std::map<int, HardwareMenu*>                  menuMap;
    std::map<int, VfdDropdown*>                   scrollListMap;   // was VfdScrollList — now VfdDropdown
    std::map<int, std::vector<WaveformButton*>>   waveMap;
    std::map<int, WaveformButton*>                ledMap;
    std::map<int, std::vector<WaveformButton*>>   radioMap;
    std::map<int, std::pair<juce::String, juce::String>> radioLabelMap; // paramID → {offLabel, onLabel}

    void updateVisualizers();
    void addRadioLEDControl  (const XpanderParam& def, MidiEngine* engine);
    void addButtonControl    (const XpanderParam& def, MidiEngine* engine);
    void addKnobControl      (const XpanderParam& def, MidiEngine* engine);
    void addMenuControl      (const XpanderParam& def, MidiEngine* engine);
    void addScrollListControl(const XpanderParam& def, MidiEngine* engine);
    void addWaveformControl  (const XpanderParam& def, MidiEngine* engine);
};

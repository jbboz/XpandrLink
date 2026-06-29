#pragma once
#include <JuceHeader.h>
#include "Engine/MidiEngine.h"
#include "UI/ModAssignmentLogic.h"
#include "Tabs/EditorTabComponent.h"
#include "UI/OberheimLookAndFeel.h"

class MainComponent : public juce::Component
{
public:
    explicit MainComponent(MidiEngine& engine, ModAssignmentLogic& modLogic);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // 1. LookAndFeel — must outlive all components
    OberheimLookAndFeel oberheimLF;

    // 2. Engine + mod logic references — owned by AudioProcessor (outlive this component)
    MidiEngine& midiEngine;
    ModAssignmentLogic& modAssignmentLogic;

    // 3. Editor — the sole UI component (library panel is now an overlay within it)
    std::unique_ptr<EditorTabComponent> editorTab;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

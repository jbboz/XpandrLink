#include "MainComponent.h"

MainComponent::MainComponent(MidiEngine& engine, ModAssignmentLogic& modLogic)
    : midiEngine(engine), modAssignmentLogic(modLogic)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&oberheimLF);

    editorTab.reset(new EditorTabComponent(midiEngine, modAssignmentLogic));
    addAndMakeVisible(*editorTab);

    setSize(1280, 800);
}

MainComponent::~MainComponent()
{
    editorTab.reset();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121212));
}

void MainComponent::resized()
{
    editorTab->setBounds(getLocalBounds());
}

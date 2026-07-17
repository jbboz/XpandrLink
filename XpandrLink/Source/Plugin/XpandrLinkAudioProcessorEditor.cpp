#include "XpandrLinkAudioProcessorEditor.h"

XpandrLinkAudioProcessorEditor::XpandrLinkAudioProcessorEditor (XpandrLinkAudioProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      mainComponent (p.getMidiEngine(), p.getModAssignmentLogic())
{
    addAndMakeVisible (mainComponent);
    setResizable (true, true);
    setResizeLimits (1024, 640, 1920, 1200);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (kDesignW / (double) kDesignH);
    setSize (kDesignW, kDesignH);
}

XpandrLinkAudioProcessorEditor::~XpandrLinkAudioProcessorEditor() {}

void XpandrLinkAudioProcessorEditor::resized()
{
    mainComponent.setBounds (0, 0, kDesignW, kDesignH);
    float scale = juce::jmin ((float) getWidth()  / kDesignW,
                              (float) getHeight() / kDesignH);
    mainComponent.setTransform (juce::AffineTransform::scale (scale));
}

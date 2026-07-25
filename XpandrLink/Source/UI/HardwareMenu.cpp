#include "HardwareMenu.h"

HardwareMenu::HardwareMenu(const juce::String& name, ModAssignmentLogic* modLogic)
    : modAssignmentLogic(modLogic)
{
    menuLF.modLogic = modLogic;
    setName(name);
    setLookAndFeel(&menuLF);
    setJustificationType(juce::Justification::centred);
    setTextWhenNothingSelected("Select");
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
}

HardwareMenu::~HardwareMenu() { setLookAndFeel(nullptr); }

void HardwareMenu::mouseDown(const juce::MouseEvent& e)
{
    if (modAssignmentLogic && modAssignmentLogic->isRouting())
    {
        if (modAssignmentLogic->isValidModDestination(paramID))
        {
            if (onAssignmentCallback) onAssignmentCallback();
            if (auto* top = getTopLevelComponent()) top->repaint();
        }
        return;
    }
    if (modAssignmentLogic) modAssignmentLogic->notifyDestinationFocused(paramID);
    juce::ComboBox::mouseDown(e);
}

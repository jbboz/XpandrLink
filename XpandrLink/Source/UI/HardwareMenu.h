#pragma once
#include <JuceHeader.h>
#include "ModAssignmentLogic.h"
#include "HardwareComboBoxLookAndFeel.h"

// --- HARDWARE MENU (Updated to use LF) ---
class HardwareMenu : public juce::ComboBox
{
public:
    HardwareMenu(const juce::String& name, ModAssignmentLogic* modLogic = nullptr);
    ~HardwareMenu() override;

    int paramID = -1;
    void setParamID(int pid) { paramID = pid; getProperties().set("paramID", pid); }
    std::function<void()> onAssignmentCallback;

    void mouseDown(const juce::MouseEvent& e) override;

private:
    ModAssignmentLogic* modAssignmentLogic = nullptr;
    HardwareComboBoxLookAndFeel menuLF;
};

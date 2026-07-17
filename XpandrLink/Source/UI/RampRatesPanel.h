/*
  RampRatesPanel.h
  Shows all 4 RAMP rate knobs side-by-side in a single non-tabbed panel.
  Matches the HTML bRAMPS() panel in Task4-Full-Editor-v4.
  Param IDs: 260 (RAMP1 rate), 270 (RAMP2), 280 (RAMP3), 290 (RAMP4).
*/
#pragma once
#include <JuceHeader.h>
#include "HardwareComponents.h"
#include "ThemeData.h"
#include "ModAssignmentLogic.h"
#include "../Data/XpanderData.h"
#include "../Engine/MidiEngine.h"

class RampRatesPanel : public juce::Component
{
public:
    explicit RampRatesPanel(MidiEngine* engine, ModAssignmentLogic* modLogic = nullptr)
    {
        // Build one HardwareKnob per RAMP rate param.
        for (const auto& def : XpanderParams)
        {
            if (def.group != "RAMP 1" && def.group != "RAMP 2" &&
                def.group != "RAMP 3" && def.group != "RAMP 4") continue;
            if (def.choices.size() > 0 || def.isButton || def.isBitmask || def.isRadioLED) continue;
            // Only the Rate param (non-button, no choices) for each RAMP group.
            auto* k = new HardwareKnob("RAMP" + juce::String(rampIndex + 1));
            k->setRange(def.min, def.max, 1);
            k->paramID = def.id;
            if (modLogic)
                k->onAssignmentCallback = [def, engine, modLogic] { modLogic->assignToDestination(def.id, engine); };
            k->onValueChange = [engine, def, k] {
                engine->sendParameterToSynth(def.page, def.paramCol, (int)k->getValue(), false);
            };
            addAndMakeVisible(k);
            knobs[rampIndex] = k;
            knobIDs[rampIndex] = def.id;
            knobList.add(k);
            ++rampIndex;
            if (rampIndex >= 4) break;
        }
    }

    void updateParameter(int paramID, int value)
    {
        for (int i = 0; i < 4; ++i)
            if (knobs[i] && knobIDs[i] == paramID)
            {
                knobs[i]->setValue(value, juce::dontSendNotification);
                knobs[i]->repaint();
                return;
            }
    }

    int getParameterValue(int paramID)
    {
        for (int i = 0; i < 4; ++i)
            if (knobs[i] && knobIDs[i] == paramID)
                return (int)knobs[i]->getValue();
        return -999;
    }

    void registerParams(const std::function<void(int)>& cb) const {
        for (int i = 0; i < 4; ++i)
            if (knobs[i]) cb(knobIDs[i]);
    }

    void paint(juce::Graphics& g) override
    {
        auto theme = ThemeData::getHardwareTheme();
        auto b = getLocalBounds().toFloat();

        g.setColour(theme.groupOutline);
        g.drawRect(b, 1.0f);

        g.setColour(theme.headerBand);
        g.fillRect(1.0f, 1.0f, b.getWidth() - 2.0f, 20.0f);

        g.setColour(theme.textLabel);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("RAMPS", juce::Rectangle<int>(10, 0, 80, 20), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedTop(20).reduced(6);
        int n = knobList.size();
        if (n == 0) return;
        const int kW = 50, kH = 76, kGap = 14;
        int totalW  = n * kW + (n - 1) * kGap;
        int startX  = area.getX() + juce::jmax(0, (area.getWidth() - totalW) / 2);
        int yOffset = juce::jmax(0, (area.getHeight() - kH) / 2);
        for (int i = 0; i < n; ++i)
            knobList[i]->setBounds(startX + i * (kW + kGap),
                                   area.getY() + yOffset, kW, kH);
    }

private:
    HardwareKnob* knobs[4]  = {};
    int           knobIDs[4] = {};
    int           rampIndex  = 0;

    juce::OwnedArray<HardwareKnob> knobList;
};

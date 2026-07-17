/*
  FmLagPanel.h
  Combined FM + LAG panel. All controls 50×72 (label 12 + VFD 16 + body 44)
  so labels, VFDs, and controls align horizontally across the row.

  FM section (~28%):  [AMP knob 50×72] [DEST dropdown 50×72]
  LAG section (~72%): [LAG IN dropdown 50×72] [RATE knob 50×72] | toggles column
*/
#pragma once
#include <JuceHeader.h>
#include "HardwareComponents.h"
#include "ModAssignmentLogic.h"
#include "ThemeData.h"
#include "../Data/XpanderData.h"

class FmLagPanel : public juce::Component
{
public:
    FmLagPanel(MidiEngine* engine, ModAssignmentLogic* modLogic = nullptr)
    {
        for (const auto& def : XpanderParams)
        {
            if (def.id == 60)  // FM_AMP — standard knob
            {
                fmAmp.reset(new HardwareKnob("Amp"));
                fmAmp->setRange(def.min, def.max, 1);
                fmAmp->paramID = def.id;
                auto* k = fmAmp.get();
                fmAmp->onAssignmentCallback = [def, engine, modLogic] {
                    if (modLogic) modLogic->assignToDestination(def.id, engine);
                };
                fmAmp->onValueChange = [engine, def, k] {
                    engine->sendParameterToSynth(def.page, def.paramCol, (int)k->getValue(), false);
                };
                addAndMakeVisible(fmAmp.get());
            }
            else if (def.id == 61)  // FM_DEST — WLIST radio: VCO / VCF
            {
                for (int i = 0; i < (int)def.choices.size(); ++i)
                {
                    auto* b = new WaveformButton(def.choices[i]);
                    b->setClickingTogglesState(false);
                    fmDestOwned.add(b);
                    addAndMakeVisible(b);
                }
                if (!fmDestOwned.isEmpty())
                    fmDestOwned[0]->setToggleState(true, juce::dontSendNotification);
                for (int i = 0; i < fmDestOwned.size(); ++i)
                {
                    int idx = i;
                    fmDestOwned[i]->onClick = [this, engine, def, idx] {
                        for (int j = 0; j < fmDestOwned.size(); ++j) {
                            fmDestOwned[j]->setToggleState(j == idx, juce::dontSendNotification);
                            fmDestOwned[j]->repaint();
                        }
                        engine->sendParameterToSynth(def.page, def.paramCol, idx, true);
                    };
                }
            }
            else if (def.id == 62)  // LAG_IN — VfdDropdown (27 sources)
            {
                lagIn.reset(new VfdDropdown("Lag In"));
                lagIn->setItems(def.choices);
                lagIn->paramID = def.id;
                lagIn->onAssignmentCallback = [def, engine, modLogic] {
                    if (modLogic) modLogic->assignToDestination(def.id, engine);
                };
                lagIn->onChange = [engine, def](int idx) {
                    engine->sendParameterToSynth(def.page, def.paramCol, idx, false);
                };
                addAndMakeVisible(lagIn.get());
            }
            else if (def.id == 63)  // LAG_RATE — standard knob
            {
                lagRate.reset(new HardwareKnob("Rate"));
                lagRate->setRange(def.min, def.max, 1);
                lagRate->paramID = def.id;
                auto* k = lagRate.get();
                lagRate->onAssignmentCallback = [def, engine, modLogic] {
                    if (modLogic) modLogic->assignToDestination(def.id, engine);
                };
                lagRate->onValueChange = [engine, def, k] {
                    engine->sendParameterToSynth(def.page, def.paramCol, (int)k->getValue(), false);
                };
                addAndMakeVisible(lagRate.get());
            }
            else if (def.id == 64)  // LAG_LEGATO — WLIST style (LED + VFD box + VFD font)
            {
                legato.reset(new WaveformButton("Legato"));
                legato->setClickingTogglesState(true);
                legato->getProperties().set("isLed", true);
                legato->useWlistStyle = true;
                auto* b = legato.get();
                legato->onClick = [engine, def, b] {
                    engine->sendParameterToSynth(def.page, def.paramCol, b->getToggleState() ? 1 : 0, true);
                };
                addAndMakeVisible(legato.get());
            }
            else if (def.id == 65)  // LAG_LIN_EXP — WLIST style (LED + VFD box + VFD font)
            {
                linExpo.reset(new WaveformButton("Linear"));
                linExpo->setClickingTogglesState(true);
                linExpo->getProperties().set("isLed", true);
                linExpo->useWlistStyle = true;
                linExpo->setToggleState(true, juce::dontSendNotification); // default: Linear active
                auto* b = linExpo.get();
                linExpo->onClick = [this, engine, def, b] {
                    bool isLinear = b->getToggleState();
                    b->setName(isLinear ? "Linear" : "Expo");
                    b->repaint();
                    engine->sendParameterToSynth(def.page, def.paramCol, isLinear ? 0 : 1, true);
                    if (eqTime) eqTime->setEnabled(isLinear);
                };
                addAndMakeVisible(linExpo.get());
            }
            else if (def.id == 66)  // LAG_EQUAL_TIME — WLIST style (LED + VFD box + VFD font)
            {
                eqTime.reset(new WaveformButton("=Time"));
                eqTime->setClickingTogglesState(true);
                eqTime->getProperties().set("isLed", true);
                eqTime->useWlistStyle = true;
                auto* b = eqTime.get();
                eqTime->onClick = [engine, def, b] {
                    engine->sendParameterToSynth(def.page, def.paramCol, b->getToggleState() ? 1 : 0, true);
                };
                addAndMakeVisible(eqTime.get());
            }
        }
    }

    void updateParameter(int paramID, int value)
    {
        if (paramID == 60 && fmAmp)   { fmAmp->setValue(value, juce::dontSendNotification); fmAmp->repaint(); }
        if (paramID == 61 && fmDestOwned.size() == 2) {
            fmDestOwned[0]->setToggleState(value == 0, juce::dontSendNotification);
            fmDestOwned[1]->setToggleState(value == 1, juce::dontSendNotification);
            fmDestOwned[0]->repaint(); fmDestOwned[1]->repaint();
        }
        if (paramID == 62 && lagIn)   { lagIn->setSelectedIndex(value); }
        if (paramID == 63 && lagRate) { lagRate->setValue(value, juce::dontSendNotification); lagRate->repaint(); }
        if (paramID == 64 && legato)  { legato->setToggleState(value != 0, juce::dontSendNotification); legato->repaint(); }
        if (paramID == 65 && linExpo) {
            bool isLinear = (value == 0);
            linExpo->setToggleState(isLinear, juce::dontSendNotification);
            linExpo->setName(isLinear ? "Linear" : "Expo");
            linExpo->repaint();
            if (eqTime) eqTime->setEnabled(isLinear);
        }
        if (paramID == 66 && eqTime)  { eqTime->setToggleState(value != 0, juce::dontSendNotification); eqTime->repaint(); }
    }

    int getParameterValue(int paramID)
    {
        if (paramID == 60 && fmAmp)   return (int)fmAmp->getValue();
        if (paramID == 61 && fmDestOwned.size() == 2)
            return fmDestOwned[1]->getToggleState() ? 1 : 0;
        if (paramID == 62 && lagIn)   return lagIn->getSelectedIndex();
        if (paramID == 63 && lagRate) return (int)lagRate->getValue();
        if (paramID == 64 && legato)  return legato->getToggleState() ? 1 : 0;
        if (paramID == 65 && linExpo)
            return linExpo->getToggleState() ? 0 : 1;  // ON=Linear(0), OFF=Expo(1)
        if (paramID == 66 && eqTime)  return eqTime->getToggleState() ? 1 : 0;
        return -999;
    }

    void registerParams(const std::function<void(int)>& cb) const {
        for (int id : { 60, 61, 62, 63, 64, 65, 66 }) cb(id);
    }

    void paint(juce::Graphics& g) override
    {
        auto theme = ThemeData::getHardwareTheme();
        auto b     = getLocalBounds().toFloat();

        g.setColour(theme.groupOutline);
        g.drawRect(b, 1.0f);

        // Split header band
        auto header = b.removeFromTop(20.0f);
        g.setColour(theme.headerBand);
        g.fillRect(header);

        float fmW = header.getWidth() * kFmFrac;
        g.setColour(theme.textLabel);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        auto fmHdr = header.removeFromLeft(fmW);
        g.drawText("FM",  fmHdr.withTrimmedLeft(10).toNearestInt(), juce::Justification::centredLeft, false);
        g.drawText("LAG", header.withTrimmedLeft(10).toNearestInt(), juce::Justification::centredLeft, false);

        // Vertical divider
        g.setColour(theme.groupOutline);
        g.drawVerticalLine((int)(getWidth() * kFmFrac), 20.0f, (float)getHeight());
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop(20); // header band
        area.reduce(4, 6);

        int totalW = area.getWidth();
        int fmEndX = (int)(totalW * kFmFrac);

        // --- FM section ---
        auto fmArea = area.removeFromLeft(fmEndX);
        area.removeFromLeft(4); // gap

        // AMP knob + DEST WLIST side by side
        const int ampW = 50, ampH = 76;
        int fmY = fmArea.getCentreY() - ampH / 2;
        if (fmAmp)
            fmAmp->setBounds(fmArea.getX() + 2, fmY, ampW, ampH);

        // FM DEST radio list (2 WaveformButtons)
        const int fmBtnH = 26, destX = fmArea.getX() + ampW + 8;
        int destStartY = fmArea.getCentreY() - (fmBtnH * 2 + 3) / 2;
        for (int i = 0; i < fmDestOwned.size(); ++i)
            fmDestOwned[i]->setBounds(destX, destStartY + i * (fmBtnH + 3), 52, fmBtnH);

        // --- LAG section — centered group, 14px gaps (matches LFO compact layout) ---
        auto lagArea = area;
        const int lagInW  = 76;
        const int rateW   = 50;
        const int itemGap = 14;
        const int totalGroupW = lagInW + itemGap + rateW + itemGap + kToggleW;
        int lagStartX = lagArea.getX() + juce::jmax(0, (lagArea.getWidth() - totalGroupW) / 2);
        int lagCY = lagArea.getCentreY();

        if (lagIn)
            lagIn->setBounds(lagStartX, lagCY - 38, lagInW, 76);
        if (lagRate)
            lagRate->setBounds(lagStartX + lagInW + itemGap, lagCY - 38, rateW, 76);

        // Single-column toggle stack: =Time / Legato / Lin·Exp
        int toggleX = lagStartX + lagInW + itemGap + rateW + itemGap;
        const int btnH = 26, btnGap = 4;
        int totalBtnH = 3 * btnH + 2 * btnGap;
        int ty = lagCY - totalBtnH / 2;
        if (eqTime)  eqTime->setBounds(toggleX, ty, kToggleW, btnH);
        ty += btnH + btnGap;
        if (legato)  legato->setBounds(toggleX, ty, kToggleW, btnH);
        ty += btnH + btnGap;
        if (linExpo) linExpo->setBounds(toggleX, ty, kToggleW, btnH);
    }

private:
    static constexpr float kFmFrac   = 0.33f;  // FM gets more width so the VCO/VCF DEST
                                               // buttons clear the divider; LAG (which has
                                               // slack) narrows to accommodate.
    static constexpr int   kToggleW  = 80;

    // FM controls
    std::unique_ptr<HardwareKnob>   fmAmp;
    juce::OwnedArray<WaveformButton> fmDestOwned;  // WLIST radio: VCO / VCF

    // LAG controls
    std::unique_ptr<VfdDropdown>    lagIn;
    std::unique_ptr<HardwareKnob>   lagRate;
    std::unique_ptr<WaveformButton> eqTime, legato, linExpo;
};

/*
  VoiceArchitectureComponent.h
  Version: 24.0 (3-Column Grid: Task4-Full-Editor-v4 design)

  Layout (3 equal columns × 3 rows):
    Col 0         Col 1         Col 2
    VCO 1         FILTER        ENV (tabbed)
    VCO 2         MODULATION    TRACK (tabbed)
    FM + LAG      RAMPS         LFO (tabbed)
*/
#pragma once
#include <JuceHeader.h>
#include <unordered_map>
#include <unordered_set>
#include "ModulePanel.h"
#include "GroupTabPanel.h"
#include "ModSummaryPanel.h"
#include "FmLagPanel.h"
#include "RampRatesPanel.h"
#include "../Engine/MidiEngine.h"

class VoiceArchitectureComponent : public juce::Component
{
public:
    VoiceArchitectureComponent(MidiEngine* engine, ModAssignmentLogic* modLogic)
        : modAssignmentLogic(modLogic)
    {
        // --- Column 0 ---
        vco1.reset(new ModulePanel("VCO 1", "VCO 1", engine, "", modLogic));
        addAndMakeVisible(vco1.get());

        vco2.reset(new ModulePanel("VCO 2", "VCO 2", engine, "", modLogic));
        addAndMakeVisible(vco2.get());

        fmLag.reset(new FmLagPanel(engine, modLogic));
        addAndMakeVisible(fmLag.get());

        // --- Column 1 ---
        vcf.reset(new ModulePanel("FILTER", "VCF", engine, "", modLogic)); // no VCF Mod additionalGroup; flags live in PAGE2
        addAndMakeVisible(vcf.get());

        modSummary.reset(new ModSummaryPanel());
        modSummary->setMidiEngine(engine);
        addAndMakeVisible(modSummary.get());

        rampRates.reset(new RampRatesPanel(engine, modLogic));
        addAndMakeVisible(rampRates.get());

        // --- Column 2 ---
        envGroup.reset(new GroupTabPanel("ENV"));
        addAndMakeVisible(envGroup.get());
        for (int i = 1; i <= 5; ++i)
            envGroup->addTab("ENV" + juce::String(i), std::make_unique<ModulePanel>("", "ENV " + juce::String(i), engine, "", modLogic));

        trkGroup.reset(new GroupTabPanel("TRACK"));
        addAndMakeVisible(trkGroup.get());
        for (int i = 1; i <= 3; ++i)
            trkGroup->addTab("TRK" + juce::String(i), std::make_unique<ModulePanel>("", "TRACK " + juce::String(i), engine, "", modLogic));

        lfoGroup.reset(new GroupTabPanel("LFO"));
        addAndMakeVisible(lfoGroup.get());
        for (int i = 1; i <= 5; ++i) {
            auto lp = std::make_unique<ModulePanel>("", "LFO " + juce::String(i), engine, "", modLogic);
            lp->setCompactLayout(true);
            lfoGroup->addTab("LFO" + juce::String(i), std::move(lp));
        }

        // S&H In source is valid only when that LFO's Wave == "Sample" (LfoShapes idx 6).
        // Mirror the ENV LFO-Src constraint pattern: always visible, enabled on Sample.
        lfoGroup->setParameterChangedCallback([this](int, int) { reapplyAllLfoConstraints(); });
        for (int id : { 72, 82, 92, 102, 112 })
            lfoGroup->setScrollListEnabled(id, false);   // start disabled until a Sample wave loads

        // When a valid mod-destination param is clicked in the editor,
        // filter ModSummaryPanel to show mods for that destination.
        if (modLogic)
            modLogic->onDestinationFocused = [this](int destID) {
                if (modSummary) modSummary->setCurrentDestination(destID);
            };

        // ENV tab selection focuses ModSummaryPanel on that ENV's destination range.
        // ENV1 destIDs: 20-24, ENV2: 25-29, ENV3: 30-34, ENV4: 35-39, ENV5: 40-44.
        // Use the first destID in each range (ENV_DLY) as the representative focus point.
        envGroup->onTabSelected = [this](int idx) {
            if (modSummary) modSummary->setCurrentDestination(20 + idx * 5);
        };

        buildParamMap();
    }

    // --- Modulation panel delegation ---

    void addModulationEntry(int srcIdx, int destIdx, int amount, int idSource = -1)
    {
        if (modSummary) modSummary->addEntry(srcIdx, destIdx, amount, idSource);
    }

    void removeModulationEntry(int srcIdx, int destIdx)
    {
        if (modSummary) modSummary->removeEntry(srcIdx, destIdx);
    }

    int getNextIdSourceForDest(int destIdx) const
    {
        return modSummary ? modSummary->getNextIdSourceForDest(destIdx) : 0;
    }

    int getIdSourceForEntry(int srcIdx, int destIdx) const
    {
        return modSummary ? modSummary->getIdSourceForEntry(srcIdx, destIdx) : -1;
    }

    void decrementIdSourceAfterRemove(int destIdx, int removedIdSource)
    {
        if (modSummary) modSummary->decrementIdSourceAfterRemove(destIdx, removedIdSource);
    }

    // Slot-keyed API -- see ModSummaryPanel::SlotEntry for rationale.
    bool getEntryAtSlot(int destIdx, int idSource, ModSummaryPanel::SlotEntry& out) const
    {
        return modSummary ? modSummary->getEntryAtSlot(destIdx, idSource, out) : false;
    }

    void setSourceAtSlot(int destIdx, int idSource, int newSrcIdx)
    {
        if (modSummary) modSummary->setSourceAtSlot(destIdx, idSource, newSrcIdx);
    }

    void setAmountAtSlot(int destIdx, int idSource, int newAmount)
    {
        if (modSummary) modSummary->setAmountAtSlot(destIdx, idSource, newAmount);
    }

    void setQuantizeAtSlot(int destIdx, int idSource, bool quantize)
    {
        if (modSummary) modSummary->setQuantizeAtSlot(destIdx, idSource, quantize);
    }

    void removeAtSlot(int destIdx, int idSource)
    {
        if (modSummary) modSummary->removeAtSlot(destIdx, idSource);
    }

    void setOnRemoveModulation(std::function<void(int, int)> cb)
    {
        if (modSummary) modSummary->onRemoveRequested = cb;
    }

    void setOnAmountChanged(std::function<void(int, int, int)> cb)
    {
        if (modSummary) modSummary->onAmountChanged = cb;
    }

    void setOnSourceChanged(std::function<void(int, int, int, int, int)> cb)
    {
        if (modSummary) modSummary->onSourceChanged = cb;
    }

    void setOnNewSourceSelected(std::function<void(int, int)> cb)
    {
        if (modSummary) modSummary->onNewSourceSelected = cb;
    }

    void updateModSummary(const std::vector<int>& patchData)
    {
        if (modSummary) modSummary->updateFromPatch(patchData);

        std::set<int> routedParams;
        if ((int)patchData.size() >= 188)
        {
            for (int i = 0; i < 20; ++i)
            {
                int base   = 128 + i * 3;
                int src    = patchData[base];
                int amt    = patchData[base + 1];
                int dst    = patchData[base + 2];
                int absAmt = amt & 0x3F;
                bool neg   = (amt & 0x40) != 0;
                int amount = neg ? -absAmt : absAmt;
                if (src <= 26 && dst <= 46 && amount != 0)
                {
                    int pid = ModAssignmentLogic::getParamIDForDestination(dst);
                    if (pid >= 0) routedParams.insert(pid);
                }
            }
        }
        if (modAssignmentLogic) {
            // The decoded set is authoritative and REPLACES the active set so a
            // previously loaded patch's LEDs don't bleed into this one (BUG-31).
            // Recent optimistic inserts (in-flight assignments) are merged back in
            // by reconcile so they survive a stale patch dump.
            modAssignmentLogic->reconcileWithDecodedSet(routedParams);
        }
        repaint();
    }

    // --- Parameter update/query (fan out to all panels) ---

    void updateParameter(int paramID, int value)
    {
        auto it = paramPanelMap_.find(paramID);
        if (it != paramPanelMap_.end())
            it->second->updateParameter(paramID, value);
        else if (fmLagIds_.count(paramID))
            fmLag->updateParameter(paramID, value);
        else if (rampRateIds_.count(paramID))
            rampRates->updateParameter(paramID, value);

        // A Wave change (live edit or patch load) re-evaluates the S&H In gating (TASK-27).
        if (paramID == 71 || paramID == 81 || paramID == 91 || paramID == 101 || paramID == 111)
            reapplyAllLfoConstraints();
    }

    int getParameterValue(int paramID)
    {
        auto it = paramPanelMap_.find(paramID);
        if (it != paramPanelMap_.end()) return it->second->getParameterValue(paramID);
        if (fmLagIds_.count(paramID))    return fmLag->getParameterValue(paramID);
        if (rampRateIds_.count(paramID)) return rampRates->getParameterValue(paramID);
        return -999;
    }

    // --- Layout ---

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        const int gap = 10;

        int colW = (area.getWidth() - 2 * gap) / 3;

        // Fixed row heights — stable regardless of whether the bottom pane is open.
        // row1: header(20)+pad(6)+knobs(76)+gap(6)+graph(66)+pad(6) = 180px
        // row2: VCO2/MODULATION/TRACK — 185px to fit TRACK graph (~67px) below knobs
        // row3: FM+LAG/RAMPS/LFO — 130px (accommodates 26px toggle buttons in LAG)
        const int row1H = 180;
        const int row2H = 185;
        const int row3H = 130;

        // Column left edges
        int x0 = area.getX();
        int x1 = x0 + colW + gap;
        int x2 = x1 + colW + gap;

        // Row top edges
        int y0 = area.getY();
        int y1 = y0 + row1H + gap;
        int y2 = y1 + row2H + gap;

        auto cell = [&](int x, int y, int w, int h) {
            return juce::Rectangle<int>(x, y, w, h);
        };

        // Col 0
        vco1->setBounds   (cell(x0, y0, colW, row1H));
        vco2->setBounds   (cell(x0, y1, colW, row2H));
        fmLag->setBounds  (cell(x0, y2, colW, row3H));

        // Col 1
        vcf->setBounds        (cell(x1, y0, colW, row1H));
        modSummary->setBounds (cell(x1, y1, colW, row2H));
        rampRates->setBounds  (cell(x1, y2, colW, row3H));

        // Col 2
        envGroup->setBounds(cell(x2, y0, colW, row1H));
        trkGroup->setBounds(cell(x2, y1, colW, row2H));
        lfoGroup->setBounds(cell(x2, y2, colW, row3H));
    }

    void paint(juce::Graphics&) override {}

    void buildParamMap()
    {
        auto addPanel = [this](ModulePanel* p) {
            p->registerParams([this, p](int id, ModulePanel* /*same*/) {
                paramPanelMap_[id] = p;
            });
        };
        addPanel(vco1.get());
        addPanel(vco2.get());
        addPanel(vcf.get());
        trkGroup->forEachPanel(addPanel);
        envGroup->forEachPanel(addPanel);
        lfoGroup->forEachPanel(addPanel);

        // FmLagPanel and RampRatesPanel have custom (non-ModulePanel) implementations.
        fmLag->registerParams([this](int id) {
            fmLagIds_.insert(id);
        });
        rampRates->registerParams([this](int id) {
            rampRateIds_.insert(id);
        });
    }

private:
    ModAssignmentLogic* modAssignmentLogic = nullptr;

    // Routing maps built at construction — O(1) dispatch for updateParameter/getParameterValue.
    std::unordered_map<int, ModulePanel*> paramPanelMap_;
    std::unordered_set<int>               fmLagIds_;
    std::unordered_set<int>               rampRateIds_;

    // Column 0
    std::unique_ptr<ModulePanel>  vco1, vco2;
    std::unique_ptr<FmLagPanel>   fmLag;

    // Column 1
    std::unique_ptr<ModulePanel>    vcf;
    std::unique_ptr<ModSummaryPanel> modSummary;
    std::unique_ptr<RampRatesPanel>  rampRates;

    // Column 2
    std::unique_ptr<GroupTabPanel> envGroup, trkGroup, lfoGroup;

    // LFO1-5 Wave param ids and their matching S&H In source ids (TASK-27).
    static constexpr int kLfoWave_[5]   = { 71, 81, 91, 101, 111 };
    static constexpr int kLfoSample_[5] = { 72, 82, 92, 102, 112 };

    void reapplyAllLfoConstraints()
    {
        if (!lfoGroup) return;
        for (int i = 0; i < 5; ++i)
        {
            bool isSample = lfoGroup->getParameterValue(kLfoWave_[i]) == 6; // LfoShapes "Sample"
            lfoGroup->setScrollListEnabled(kLfoSample_[i], isSample);
        }
    }
};

/*
  AdvancedParamsPanel.h
  Version: 2.0 — PAGE2 layout (Task4-Full-Editor-v4 design)

  3-column layout matching the HTML buildP2():
    Left  (~30%): KB TRACKING — VCO1/VCO2/FILTER rows with mod flags
    Center (~40%): ENV tabbed (ENV1-5) — mode + trigger flags
    Right  (~30%): RAMP tabbed (RAMP1-4) stacked above LFO tabbed (LFO1-5)
*/
#pragma once
#include <JuceHeader.h>
#include "ModulePanel.h"
#include "GroupTabPanel.h"
#include "ThemeData.h"

// Inner panel that shows VCO1/VCO2/FILTER mod flags stacked as labelled rows.
class KbTrackingPanel : public juce::Component
{
public:
    explicit KbTrackingPanel(MidiEngine* engine)
    {
        vco1Mod.reset(new ModulePanel("VCO 1 MOD", "VCO 1 Mod", engine));
        vco2Mod.reset(new ModulePanel("VCO 2 MOD", "VCO 2 Mod", engine));
        vcfMod.reset (new ModulePanel("FILTER MOD","VCF Mod",   engine));

        vco1Mod->setEmbeddedMode(true);
        vco1Mod->setPage2ButtonStyle(true);
        vco2Mod->setEmbeddedMode(true);
        vco2Mod->setPage2ButtonStyle(true);
        vcfMod->setEmbeddedMode(true);
        vcfMod->setPage2ButtonStyle(true);

        addAndMakeVisible(vco1Mod.get());
        addAndMakeVisible(vco2Mod.get());
        addAndMakeVisible(vcfMod.get());
    }

    void updateParameter(int paramID, int value)
    {
        vco1Mod->updateParameter(paramID, value);
        vco2Mod->updateParameter(paramID, value);
        vcfMod->updateParameter(paramID, value);
    }

    int getParameterValue(int paramID)
    {
        int v;
        v = vco1Mod->getParameterValue(paramID); if (v != -999) return v;
        v = vco2Mod->getParameterValue(paramID); if (v != -999) return v;
        v = vcfMod->getParameterValue(paramID);  if (v != -999) return v;
        return -999;
    }

    void paint(juce::Graphics& g) override
    {
        auto theme = ThemeData::getHardwareTheme();
        auto b = getLocalBounds().toFloat();

        g.setColour(theme.groupOutline);
        g.drawRect(b, 1.0f);

        // Header band
        g.setColour(theme.headerBand);
        g.fillRect(b.removeFromTop(22.0f));
        g.setColour(theme.textLabel);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("KB TRACKING", 10, 0, getWidth() - 10, 22, juce::Justification::centredLeft);

        // Row labels — centred (both axes) in the narrow column just left of the buttons,
        // so each label sits level with its toggle-button row instead of riding the top.
        const int hdr = 22;
        int rowH = (getHeight() - hdr) / 3;
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.setColour(theme.textLabel);
        for (int i = 0; i < 3; ++i)
        {
            juce::String lbl = (i == 0) ? "VCO 1" : (i == 1) ? "VCO 2" : "FILTER";
            int y = hdr + i * rowH;
            // The embedded ModulePanel reserves 6px at the bottom of each row, centring its
            // button row in (rowH-6); match that band so the label lines up with the buttons.
            g.drawText(lbl, contentX_, y, kLabelW, rowH - 6, juce::Justification::centred);
        }
    }

    void resized() override
    {
        // Centre the [row label + 4-button group] within the block. The buttons live
        // inside an embedded ModulePanel which insets its content by kInnerPad on each
        // side, so the mod panel is positioned/sized to account for that.
        const int hdr = 22;
        int rowH = (getHeight() - hdr) / 3;

        const int btnGroupW = 4 * 66 + 3 * 6;                       // uniform P2 cells (66px) = 282
        const int visW       = kLabelW + kLabelGap + btnGroupW;      // visible content width
        const int leftPad    = juce::jmax(0, (getWidth() - visW) / 2);
        contentX_            = leftPad;                              // label x (used in paint)

        const int modX = leftPad + kLabelW + kLabelGap - kInnerPad;
        const int modW = btnGroupW + kInnerPad * 2 + 4;             // fits 4 cells in one row

        vco1Mod->setBounds(modX, hdr,           modW, rowH);
        vco2Mod->setBounds(modX, hdr + rowH,    modW, rowH);
        vcfMod->setBounds (modX, hdr + rowH*2,  modW, getHeight() - hdr - rowH*2);
    }

private:
    static constexpr int kLabelW   = 40;
    static constexpr int kLabelGap = 4;
    static constexpr int kInnerPad = 12;   // ModulePanel L/R content inset
    int contentX_ = 1;
    std::unique_ptr<ModulePanel> vco1Mod, vco2Mod, vcfMod;
};

// ============================================================================

class AdvancedParamsPanel : public juce::Component
{
public:
    explicit AdvancedParamsPanel(MidiEngine* engine)
        : midiEngine(engine)
    {
        kbTracking.reset(new KbTrackingPanel(engine));
        addAndMakeVisible(kbTracking.get());

        // ENV mode + trigger flags (including LFO Src), tabbed ENV1-5
        // isEmbed=true → compact layout to fit within PAGE2 pane height.
        envGroup.reset(new GroupTabPanel("ENV"));
        addAndMakeVisible(envGroup.get());
        for (int i = 1; i <= 5; ++i)
        {
            juce::String modeGrp = "ENV " + juce::String(i) + " Mode";
            juce::String trigGrp = "ENV " + juce::String(i) + " Trig";
            auto panel = std::make_unique<ModulePanel>("", modeGrp, engine, trigGrp);
            panel->setEmbeddedMode(true);
            panel->setPage2ButtonStyle(true);
            envGroup->addTab(juce::String(i), std::move(panel));
        }

        // When any button is clicked in the ENV panels, re-evaluate GATED/LFO Src constraints.
        envGroup->setParameterChangedCallback([this](int /*id*/, int /*val*/) {
            reapplyAllEnvConstraints();
        });

        // Sending a page-select when ENV tabs are clicked navigates the hardware display.
        envGroup->onTabSelected = [this](int index) {
            static const int envPageAddrs[] = { 0x28, 0x29, 0x2A, 0x2B, 0x2C };
            if (midiEngine && index >= 0 && index < 5)
                midiEngine->sendPageSelect(envPageAddrs[index], 0, true);
        };

        // LFO Src selectors start disabled until LFOTRIG is on.
        for (int id : { 142, 162, 182, 202, 222 })
            envGroup->setScrollListEnabled(id, false);

        // GATED starts disabled: valid only when EXTRIG or LFOTRIG is on.
        for (int id : { 143, 163, 183, 203, 223 })
            envGroup->setButtonEnabled(id, false);

        // RAMP trigger flags, tabbed RAMP1-4 (Rate knob is in main editor; only flags here)
        rampGroup.reset(new GroupTabPanel("RAMP"));
        addAndMakeVisible(rampGroup.get());
        for (int i = 1; i <= 4; ++i)
        {
            auto panel = std::make_unique<ModulePanel>("", "RAMP " + juce::String(i) + " Trig", engine);
            panel->setEmbeddedMode(true);
            panel->setPage2ButtonStyle(true);
            rampGroup->addTab(juce::String(i), std::move(panel));
        }

        // GATED follows ENV behavior: valid only when EXTRIG or LFOTRIG is on.
        rampGroup->setParameterChangedCallback([this](int, int) { reapplyAllRampConstraints(); });
        for (int id : { 265, 275, 285, 295 })
            rampGroup->setButtonEnabled(id, false);

        // LFO: Lag toggle + Retrig Mode + S&H In source (VfdDropdown, group "LFO x Adv")
        lfoGroup.reset(new GroupTabPanel("LFO"));
        addAndMakeVisible(lfoGroup.get());
        for (int i = 1; i <= 5; ++i)
        {
            auto panel = std::make_unique<ModulePanel>("", "LFO " + juce::String(i) + " Adv", engine);
            panel->setEmbeddedMode(true);
            panel->setPage2ButtonStyle(true);
            lfoGroup->addTab(juce::String(i), std::move(panel));
        }

    }

    void updateParameter(int paramID, int value)
    {
        if (kbTracking) kbTracking->updateParameter(paramID, value);
        if (envGroup)   envGroup->updateParameter(paramID, value);
        if (rampGroup)  rampGroup->updateParameter(paramID, value);
        if (lfoGroup)   lfoGroup->updateParameter(paramID, value);
        // Reapply for any ENV param so patch-load order never leaves GATED lit when invalid.
        if ((paramID >= 130 && paramID <= 143) || (paramID >= 150 && paramID <= 163) ||
            (paramID >= 170 && paramID <= 183) || (paramID >= 190 && paramID <= 203) ||
            (paramID >= 210 && paramID <= 223))
            reapplyAllEnvConstraints();
        // Same for RAMP GATED (ids 261-265 / 271-275 / 281-285 / 291-295).
        if ((paramID >= 261 && paramID <= 265) || (paramID >= 271 && paramID <= 275) ||
            (paramID >= 281 && paramID <= 285) || (paramID >= 291 && paramID <= 295))
            reapplyAllRampConstraints();
    }

    int getParameterValue(int paramID)
    {
        int v;
        if (kbTracking) { v = kbTracking->getParameterValue(paramID); if (v != -999) return v; }
        if (envGroup)   { v = envGroup->getParameterValue(paramID);   if (v != -999) return v; }
        if (rampGroup)  { v = rampGroup->getParameterValue(paramID);  if (v != -999) return v; }
        if (lfoGroup)   { v = lfoGroup->getParameterValue(paramID);   if (v != -999) return v; }
        return -999;
    }

    void paint(juce::Graphics& g) override
    {
        auto theme = ThemeData::getHardwareTheme();
        g.fillAll(theme.mainBackground);
    }

    void resized() override
    {
        // Match the main editor's 3-column grid (VoiceArchitectureComponent): same
        // reduced(10) margin, gap=10, equal thirds — so KB Tracking aligns under col 0
        // (VCO1/VCO2/FM-LAG), ENV under col 1 (FILTER/MOD/RAMPS), and RAMP+LFO under
        // col 2 (ENV/TRACK/LFO).
        auto area = getLocalBounds().reduced(10);
        const int gap  = 10;
        const int colW = (area.getWidth() - 2 * gap) / 3;

        int x0 = area.getX();
        int x1 = x0 + colW + gap;
        int x2 = x1 + colW + gap;

        kbTracking->setBounds(x0, area.getY(), colW, area.getHeight());
        envGroup->setBounds  (x1, area.getY(), colW, area.getHeight());

        // Right column: RAMP (top half) + LFO (bottom half)
        juce::Rectangle<int> rightCol(x2, area.getY(), colW, area.getHeight());
        int rampH = rightCol.getHeight() / 2 - gap / 2;
        rampGroup->setBounds(rightCol.removeFromTop(rampH));
        rightCol.removeFromTop(gap);
        lfoGroup->setBounds(rightCol);
    }

private:
    MidiEngine* midiEngine = nullptr;
    std::unique_ptr<KbTrackingPanel> kbTracking;
    std::unique_ptr<GroupTabPanel>   envGroup, rampGroup, lfoGroup;

    // ENV1-5: extrig, lfotrig, gated param IDs in order
    static constexpr int kEnvExtrig[5]  = { 140, 160, 180, 200, 220 };
    static constexpr int kEnvLfoTrig[5] = { 141, 161, 181, 201, 221 };
    static constexpr int kEnvGated[5]   = { 143, 163, 183, 203, 223 };
    static constexpr int kEnvLfoSrc[5]  = { 142, 162, 182, 202, 222 };

    void reapplyAllEnvConstraints()
    {
        if (!envGroup) return;
        for (int i = 0; i < 5; ++i)
        {
            bool extrig  = envGroup->getParameterValue(kEnvExtrig[i])  > 0;
            bool lfotrig = envGroup->getParameterValue(kEnvLfoTrig[i]) > 0;
            envGroup->setButtonEnabled(kEnvGated[i], extrig || lfotrig);
            envGroup->setScrollListEnabled(kEnvLfoSrc[i], lfotrig);
        }
    }

    // RAMP1-4: extrig, lfotrig, gated param IDs in order
    static constexpr int kRampExtrig[4]  = { 262, 272, 282, 292 };
    static constexpr int kRampLfoTrig[4] = { 263, 273, 283, 293 };
    static constexpr int kRampGated[4]   = { 265, 275, 285, 295 };

    void reapplyAllRampConstraints()
    {
        if (!rampGroup) return;
        for (int i = 0; i < 4; ++i)
        {
            bool extrig  = rampGroup->getParameterValue(kRampExtrig[i])  > 0;
            bool lfotrig = rampGroup->getParameterValue(kRampLfoTrig[i]) > 0;
            rampGroup->setButtonEnabled(kRampGated[i], extrig || lfotrig);
        }
    }
};

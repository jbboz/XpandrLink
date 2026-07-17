/*
  MorphPanel.h
  Bottom-pane UI for tone morphing (TASK-10).

  Presents two patch slots (A = current patch at open time, B = user-loaded file)
  and a 0-100 morph slider. Every slider move calls onApply with the interpolated
  param map; EditorTabComponent encodes + sends it to scratchpad 99 -- already live
  on the hardware as the slider moves, so there's no separate confirm/commit step.
  Undo reverts to Patch A (the pre-morph patch).
*/
#pragma once
#include <JuceHeader.h>
#include <array>
#include <map>
#include <memory>
#include "ThemeData.h"
#include "HardwareButtonLookAndFeel.h"
#include "../Data/PatchCodec.h"
#include "../Data/PatchMorpher.h"

class MorphPanel : public juce::Component, private juce::Timer
{
public:
    // Called on every slider move and on Apply/Cancel.
    // params: morphed {paramID→value} map (mod matrix NOT included).
    // modBytes: 60-byte mod region (always from Patch A — never morphed).
    // name: 8-char patch name for the SysEx header.
    using ApplyFn = std::function<void(const std::map<int,int>&,
                                       const std::array<uint8_t,60>&,
                                       const juce::String&)>;
    ApplyFn onApply;

    // Called when the user clicks "Load B" — owner should open the library picker
    // and call setPatchBFromSysex() with the selected patch.
    std::function<void()> onLoadBRequested;

    // -------------------------------------------------------------------------
    MorphPanel()
    {
        auto theme = ThemeData::getHardwareTheme();

        titleLabel_.setText("TONE MORPH", juce::dontSendNotification);
        titleLabel_.setColour(juce::Label::textColourId, theme.textLabel);
        titleLabel_.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        addAndMakeVisible(titleLabel_);

        for (auto* lbl : { &patchALabel_, &patchBLabel_ })
        {
            lbl->setText("-", juce::dontSendNotification);
            lbl->setColour(juce::Label::textColourId, theme.textValue.withAlpha(0.8f));
            lbl->setFont(juce::Font(juce::FontOptions(13.0f)));
            addAndMakeVisible(lbl);
        }
        patchALabel_.setJustificationType(juce::Justification::centredRight);
        patchBLabel_.setJustificationType(juce::Justification::centredLeft);

        loadBBtn_.setButtonText("Load B");
        loadBBtn_.setLookAndFeel(&lf_);
        loadBBtn_.onClick = [this] { doLoadB(); };
        addAndMakeVisible(loadBBtn_);

        // Morph factor slider 0-100.
        morphSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
        morphSlider_.setRange(0.0, 100.0, 1.0);
        morphSlider_.setValue(0.0, juce::dontSendNotification);
        morphSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 22);
        morphSlider_.setTextValueSuffix(" %");
        morphSlider_.setColour(juce::Slider::trackColourId,          theme.knobIndicatorActive.withAlpha(0.6f));
        morphSlider_.setColour(juce::Slider::thumbColourId,          theme.knobIndicatorActive);
        morphSlider_.setColour(juce::Slider::backgroundColourId,     juce::Colour(0xff2a2a2a));
        morphSlider_.setColour(juce::Slider::textBoxTextColourId,    theme.textValue);
        morphSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        morphSlider_.setEnabled(false);
        // Rate-limit hardware sends: each slider move restarts a 300ms timer.
        // At 31250 baud a 399-byte SysEx takes ~128ms to transmit; back-to-back
        // sends without this guard build a multi-second backlog that corrupts the
        // hardware's voice allocation and causes stuck notes.
        morphSlider_.onValueChange = [this] { startTimer(300); };
        addAndMakeVisible(morphSlider_);

        undoBtn_.setButtonText("UNDO");
        undoBtn_.setLookAndFeel(&lf_);
        undoBtn_.onClick = [this] { doUndo(); };
        addAndMakeVisible(undoBtn_);

        hintLabel_.setText("Load a patch B file to begin morphing", juce::dontSendNotification);
        hintLabel_.setColour(juce::Label::textColourId, theme.textLabel.withAlpha(0.4f));
        hintLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
        hintLabel_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(hintLabel_);
    }

    ~MorphPanel() override
    {
        loadBBtn_.setLookAndFeel(nullptr);
        undoBtn_.setLookAndFeel(nullptr);
    }

    // Call this when the panel becomes visible to snapshot the current patch as A.
    // sysex399: the 399-byte cached SysEx from MidiEngine::getCachedPatch().
    void setPatchA(const std::vector<uint8_t>& sysex399)
    {
        if (sysex399.size() != 399) return;
        auto raw = unpackSysex(sysex399);
        if (!raw.empty()) setPatchARaw(raw);
    }

    // Snapshot Patch A directly from the 196 decoded patch bytes (e.g. straight from
    // onPatchReceived), so loading a new patch refreshes A even while the pane is open.
    void setPatchARaw(const std::vector<uint8_t>& raw)
    {
        if (raw.size() < 196) return;

        patchA_params_ = PatchCodec::decode(raw);
        for (int i = 0; i < 60; ++i)
            patchA_mod_[(size_t)i] = raw[(size_t)(128 + i)];
        patchA_name_ = extractName(raw);

        patchALabel_.setText("A: " + patchA_name_, juce::dontSendNotification);
        hasPatchA_ = true;

        // Reset slider to 0 (full A) whenever A is re-snapshotted.
        morphSlider_.setValue(0.0, juce::dontSendNotification);
        updateControls();
    }

    // Called by EditorTabComponent after the user picks a patch from the library.
    void setPatchBFromSysex(const std::vector<uint8_t>& sysex399)
    {
        auto raw = unpackSysex(sysex399);
        if (raw.empty()) return;
        patchB_params_ = PatchCodec::decode(raw);
        for (int i = 0; i < 60; ++i)
            patchB_mod_[(size_t)i] = raw[(size_t)(128 + i)];
        patchB_name_   = extractName(raw);
        patchBLabel_.setText("B: " + patchB_name_, juce::dontSendNotification);
        hasPatchB_ = true;
        updateControls();
        if (hasPatchA_) applyMorph();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(ThemeData::getHardwareTheme().mainBackground);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12, 8);

        // Row 1: title (left) | Load B button (right)
        {
            auto row = area.removeFromTop(24);
            loadBBtn_.setBounds(row.removeFromRight(80));
            row.removeFromRight(8);
            titleLabel_.setBounds(row);
        }
        area.removeFromTop(4);

        // Row 2: Patch A name | Patch B name — centered as a pair in the morph area.
        {
            auto row = area.removeFromTop(20);
            const int lblW = 180, gap = 16;
            auto group = row.withSizeKeepingCentre(juce::jmin(row.getWidth(), lblW * 2 + gap),
                                                   row.getHeight());
            int half = (group.getWidth() - gap) / 2;
            patchALabel_.setBounds(group.removeFromLeft(half));
            group.removeFromLeft(gap);
            patchBLabel_.setBounds(group);
        }
        area.removeFromTop(8);

        // Row 3: morph slider — centered, capped width (TASK-24; grown 50% per request).
        // 330px track + 44px text box; remainder of the row stays empty.
        {
            auto row = area.removeFromTop(28);
            int sw = juce::jmin(row.getWidth(), 396);
            morphSlider_.setBounds(row.withSizeKeepingCentre(sw, row.getHeight()));
        }
        area.removeFromTop(4);

        // Row 5: hint label (shown when B not loaded)
        hintLabel_.setBounds(area.removeFromTop(20));
        area.removeFromTop(8);

        // Row 6: action button (just Undo -- Apply was removed, see doUndo/applyMorph)
        auto btnRow = area.removeFromTop(28);
        undoBtn_.setBounds(btnRow.withSizeKeepingCentre(juce::jmin(btnRow.getWidth(), 160), btnRow.getHeight()));
    }

private:
    // ---- helpers -------------------------------------------------------------

    static std::vector<uint8_t> unpackSysex(const std::vector<uint8_t>& sysex399)
    {
        // Layout: F0 10 id 01 00 progNum [392 packed bytes] F7
        if (sysex399.size() != 399 || sysex399[0] != 0xF0 || sysex399[398] != 0xF7)
            return {};
        const uint8_t* packed = sysex399.data() + 6;
        std::vector<uint8_t> raw(196);
        for (int i = 0; i < 196; ++i)
        {
            uint8_t lo = packed[2 * i];
            uint8_t hi = packed[2 * i + 1];
            raw[(size_t)i] = (uint8_t)(((hi & 0x01) << 7) | (lo & 0x7F));
        }
        return raw;
    }

    static juce::String extractName(const std::vector<uint8_t>& raw196)
    {
        if (raw196.size() < 196) return "--------";
        juce::String name;
        for (int i = 188; i < 196; ++i)
            name += (char)(raw196[(size_t)i] & 0x7F);
        name = name.trimEnd();
        return name.isEmpty() ? "--------" : name;
    }

    void doLoadB()
    {
        if (onLoadBRequested) onLoadBRequested();
    }

    void timerCallback() override
    {
        stopTimer();
        applyMorph();
    }

    void applyMorph()
    {
        if (!hasPatchA_ || !hasPatchB_ || !onApply) return;
        float factor = (float)(morphSlider_.getValue() / 100.0);
        auto params  = PatchMorpher::morph(patchA_params_, patchB_params_, factor);
        // Mod matrix also crosses at 0.5 so factor=1.0 is fully Patch B.
        const auto& mod = (factor >= 0.5f) ? patchB_mod_ : patchA_mod_;
        onApply(params, mod, morphName());
    }

    // Interim morph patch name: "M-" + first six chars of Patch A's name (8 chars max).
    juce::String morphName() const
    {
        return ("M-" + patchA_name_.substring(0, 6)).toUpperCase();
    }

    // Undo the morph: restore Patch A (the pre-morph patch) on the editor + hardware.
    void doUndo()
    {
        if (!hasPatchA_ || !onApply) return;
        morphSlider_.setValue(0.0, juce::dontSendNotification);
        // Re-apply Patch A directly (bypasses morph to avoid rounding on factor=0).
        onApply(patchA_params_, patchA_mod_, patchA_name_);
    }

    void updateControls()
    {
        bool ready = hasPatchA_ && hasPatchB_;
        morphSlider_.setEnabled(ready);
        hintLabel_.setVisible(!ready);
    }

    // -------------------------------------------------------------------------
    HardwareButtonLookAndFeel lf_;  // must outlive buttons

    juce::Label      titleLabel_, patchALabel_, patchBLabel_, hintLabel_;
    juce::TextButton loadBBtn_, undoBtn_;
    juce::Slider     morphSlider_;

    bool hasPatchA_ = false;
    bool hasPatchB_ = false;

    std::map<int,int>      patchA_params_, patchB_params_;
    std::array<uint8_t,60> patchA_mod_{}, patchB_mod_{};
    juce::String           patchA_name_, patchB_name_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphPanel)
};

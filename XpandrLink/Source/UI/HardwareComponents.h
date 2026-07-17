/*
  HardwareComponents.h
  Version: 30.0 (Split into .h/.cpp — TASK-13a)
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"
#include "OberheimLookAndFeel.h"
#include "ModAssignmentLogic.h"

// --- 1. SLIDER LF (Kills Text Box) ---
class NoTextBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Label* createSliderTextBox(juce::Slider&) override {
        auto* l = new juce::Label(); l->setVisible(false); return l;
    }
};

// --- 2. COMBOBOX LF (Fixes Double Text) ---
class HardwareComboBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModAssignmentLogic* modLogic = nullptr;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    // Kill the default label that might pop up
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(0.0f)); }
};

// --- 3. VFD KNOB ---
class HardwareKnob : public juce::Slider
{
public:
    HardwareKnob(const juce::String& labelText, ModAssignmentLogic* modLogic = nullptr);
    ~HardwareKnob() override;

    int paramID = -1;
    std::function<void()> onAssignmentCallback;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void paint(juce::Graphics& g) override;

private:
    juce::String name;
    ModAssignmentLogic* modAssignmentLogic = nullptr;
    NoTextBoxLookAndFeel noTextLF;

    struct NumericEntry : public juce::Component
    {
        juce::TextEditor ed;

        NumericEntry(int current, int minV, int maxV, std::function<void(int)> apply)
        {
            ed.setText(juce::String(current));
            ed.selectAll();
            ed.setInputRestrictions(6, "-0123456789");
            ed.setFont(ThemeData::getVfdFont(13.0f));
            addAndMakeVisible(ed);
            setSize(72, 24);

            ed.onReturnKey = [this, minV, maxV, apply]()
            {
                int val = juce::jlimit(minV, maxV, ed.getText().getIntValue());
                apply(val);
                if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                    cb->dismiss();
            };
            ed.onEscapeKey = [this]()
            {
                if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
                    cb->dismiss();
            };
        }

        void resized() override { ed.setBounds(getLocalBounds()); }
        void visibilityChanged() override { if (isVisible()) ed.grabKeyboardFocus(); }
    };
};

// --- 4. HARDWARE MENU (Updated to use LF) ---
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

// --- 5b. VFD POPUP LIST — scrollable overlay opened by VfdDropdown and FullModMatrixPanel ---
// Positioned just below 'below' (screen rect of the invoking VFD box).
// Caller must: addToDesktop(windowIsTemporary); setVisible(true); toFront(true).
class VfdPopupList : public juce::Component, private juce::Timer
{
public:
    VfdPopupList(const juce::StringArray& items, int sel,
                 juce::Rectangle<int> below,
                 std::function<void(int)> cb);

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override;
    void inputAttemptWhenModal() override { dismiss(); }
    // focusLost can fire from setVisible(false) on macOS; guard prevents double-free.
    void focusLost(FocusChangeType) override { dismiss(); }
    // Timer: dismiss when the host app loses foreground so the popup doesn't
    // float over other macOS applications.
    void timerCallback() override;

    // Convenience factory: construct, add to desktop, and make visible in one call.
    static void show(const juce::StringArray& items, int sel,
                     juce::Rectangle<int> below, std::function<void(int)> cb);

private:
    juce::StringArray items_;
    int selectedIdx_;
    int hoverIdx_     = -1;
    int scrollOffset_ = 0;
    bool dismissed_   = false;
    std::function<void(int)> callback_;

    int visibleCount() const { return juce::jmax(1, (getHeight() - 8) / 18); }
    int hitItem(int y) const;
    void dismiss();
};

// --- 6. VFD DROPDOWN — shows one selected item, click opens popup VfdScrollList ---
class VfdDropdown : public juce::Component
{
public:
    VfdDropdown(const juce::String& labelText, ModAssignmentLogic* modLogic = nullptr);

    int paramID = -1;
    std::function<void(int)>  onChange;
    std::function<void()>     onAssignmentCallback;

    void setItems(const juce::StringArray& newItems);
    void setSelectedIndex(int idx);

    int getSelectedIndex() const { return selectedIdx; }
    bool hasLabel() const { return name.isNotEmpty(); }
    int  maxItemLength() const { int m = 0; for (auto& s : items) m = juce::jmax(m, s.length()); return m; }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    juce::String        name;
    ModAssignmentLogic* modAssignmentLogic = nullptr;
    juce::StringArray   items;
    int selectedIdx = 0;
};

// --- 7. WAVEFORM BUTTON ---
// Style matrix — always use the factory methods below instead of setting flags directly:
//   WLIST: useWlistStyle=true + isLed=true  →  9px LED + VFD box + VFD font + underscore active
//          Used by: VCO1/2 waveforms, VCO2 SYNC, LAG =Time/LEGATO/LINEAR
//   P2:    useP2Style=true                  →  VFD box + VFD font + underscore active, no LED
//          Used by: AdvancedParamsPanel (all Page-2 ENV/RAMP/LFO/KB panels)
//
// LAYOUT: wlistButtonMode=true on ModulePanel is required for WLIST/P2 buttons to join
//         the flex row. FmLagPanel uses fixed layout — only paint flags matter there.
class WaveformButton : public juce::Button
{
public:
    WaveformButton(const juce::String& name) : juce::Button(name) {}

    bool useWlistStyle = false;
    bool useP2Style    = false;

    // Factory helpers — set all required flags in one call.
    static WaveformButton* makeWlistButton(const juce::String& label);
    static WaveformButton* makeP2Button(const juce::String& label);

    void paintButton(juce::Graphics& g, bool, bool) override;
};

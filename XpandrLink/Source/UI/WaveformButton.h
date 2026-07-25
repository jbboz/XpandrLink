#pragma once
#include <JuceHeader.h>

// --- WAVEFORM BUTTON ---
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

#pragma once
#include <JuceHeader.h>
#include "../Data/XpanderData.h"
#include <unordered_map>

// Builds the AudioProcessor parameter list from XpanderParams and provides O(1) lookup
// in both directions: (page, paramCol) ↔ AU parameter index.
//
// Call the constructor exactly once, from XpandrLinkAudioProcessor's constructor,
// before any other parameter access.
class XpandrLinkParameterBridge
{
public:
    struct ParamEntry {
        int  page;
        int  paramCol;
        int  minVal;
        int  maxVal;
        bool isButton;
    };

    explicit XpandrLinkParameterBridge (juce::AudioProcessor& processor)
    {
        int auIndex = 0;

        for (const auto& p : XpanderParams)
        {
            if (p.isBitmask)
            {
                // Each bit in the bitmask gets its own AudioParameterBool.
                // The hardware sends individual button SysEx per bit
                // (paramCol + N for bit N), so each maps to its own lookup key.
                int numBits = p.choices.size();
                for (int bit = 0; bit < numBits; ++bit)
                {
                    juce::String name = p.group + ": " + p.name + " " + p.choices[bit];
                    processor.addParameter (new juce::AudioParameterBool (
                        juce::ParameterID (name, 1), name, false));

                    ParamEntry e { p.page, p.paramCol + bit, 0, 1, true };
                    lookup[makeKey (p.page, p.paramCol + bit)] = auIndex;
                    entries.push_back (e);
                    ++auIndex;
                }
            }
            else if (!p.choices.isEmpty() || p.isRadioLED)
            {
                // Choice list (including radio-LED pairs).
                // isButton flag is preserved so the SysEx subpage is correct.
                juce::String name = p.group + ": " + p.name;
                processor.addParameter (new juce::AudioParameterChoice (
                    juce::ParameterID (name, 1), name, p.choices, 0));

                ParamEntry e { p.page, p.paramCol, p.min, p.max, p.isButton };
                lookup[makeKey (p.page, p.paramCol)] = auIndex;
                entries.push_back (e);
                ++auIndex;
            }
            else if (p.isButton)
            {
                juce::String name = p.group + ": " + p.name;
                processor.addParameter (new juce::AudioParameterBool (
                    juce::ParameterID (name, 1), name, false));

                ParamEntry e { p.page, p.paramCol, 0, 1, true };
                lookup[makeKey (p.page, p.paramCol)] = auIndex;
                entries.push_back (e);
                ++auIndex;
            }
            else
            {
                // Continuous float (knobs/sliders).
                juce::String name = p.group + ": " + p.name;
                processor.addParameter (new juce::AudioParameterFloat (
                    juce::ParameterID (name, 1), name,
                    juce::NormalisableRange<float> ((float)p.min, (float)p.max, 1.0f),
                    (float)p.min));

                ParamEntry e { p.page, p.paramCol, p.min, p.max, false };
                lookup[makeKey (p.page, p.paramCol)] = auIndex;
                entries.push_back (e);
                ++auIndex;
            }
        }
    }

    // Returns AU parameter index for (page, paramCol), or -1 if not registered.
    int findParamIndex (int page, int paramCol) const
    {
        auto it = lookup.find (makeKey (page, paramCol));
        return (it != lookup.end()) ? it->second : -1;
    }

    const ParamEntry* getEntry (int auIndex) const
    {
        if (auIndex < 0 || auIndex >= (int)entries.size()) return nullptr;
        return &entries[(size_t)auIndex];
    }

    // Hardware integer value → normalized 0..1 (for setValueNotifyingHost).
    float normalize (const ParamEntry& e, int hwValue) const
    {
        if (e.maxVal == e.minVal) return 0.0f;
        return juce::jlimit (0.0f, 1.0f,
            (float)(hwValue - e.minVal) / (float)(e.maxVal - e.minVal));
    }

    // Normalized 0..1 → hardware integer value (for sendParameterToSynth).
    int denormalize (const ParamEntry& e, float normalized) const
    {
        return e.minVal + (int)std::round (normalized * (float)(e.maxVal - e.minVal));
    }

private:
    static uint32_t makeKey (int page, int paramCol)
    {
        return (uint32_t)((page << 16) | (paramCol & 0xFFFF));
    }

    std::vector<ParamEntry>            entries;
    std::unordered_map<uint32_t, int>  lookup;
};

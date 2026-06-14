/*
  PatchMorpher.h
  Continuous interpolation between two Xpander / Matrix-12 patches (TASK-10).

  Pure logic (no UI / no JUCE dependency beyond juce::jlimit). Operates on the
  same {paramID -> value} maps that PatchCodec produces. The mod matrix (bytes
  128-187 of the 196-byte decoded form) is always copied from Patch A — morphing
  two independent mod matrices produces nonsense (C# reference comment).

  Algorithm mirrors XpanderTone.MorphTones in the C# reference:
  - Continuous params: result = A + round((B - A) * factor), clamped to [min,max].
  - Discrete params (choices, buttons, bitmask, radioLED): always copy from A.
*/
#pragma once
#include <map>
#include <cmath>
#include <JuceHeader.h>
#include "XpanderData.h"
#include "PatchRandomizer.h"   // for isContinuous()

namespace PatchMorpher
{
    // Returns an interpolated param map. factor 0.0 = Patch A, 1.0 = Patch B.
    // Mod matrix bytes are NOT in the returned map; caller handles them separately.
    inline std::map<int,int> morph(const std::map<int,int>& a,
                                   const std::map<int,int>& b,
                                   float factor)
    {
        // Discrete params (waveforms, modes, bitmasks, buttons) cross from A to B at the
        // midpoint so that factor=0 is fully A and factor=1 is fully B.
        const std::map<int,int>& discreteSrc = (factor >= 0.5f) ? b : a;
        std::map<int,int> result = discreteSrc;  // seed with appropriate source for discrete

        for (const auto& def : XpanderParams)
        {
            if (!PatchRandomizer::isContinuous(def)) continue;  // discrete: already seeded
            auto itA = a.find(def.id);
            if (itA == a.end()) continue;
            int valA = itA->second;
            auto itB = b.find(def.id);
            int valB = (itB != b.end()) ? itB->second : valA;
            int v = valA + (int)std::round((valB - valA) * (double)factor);
            result[def.id] = juce::jlimit(def.min, def.max, v);
        }
        return result;
    }
} // namespace PatchMorpher

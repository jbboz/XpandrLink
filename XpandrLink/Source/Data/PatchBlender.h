/*
  PatchBlender.h
  Collapse a weighted set of patches into one patch for the Timbre Space.
  Continuous params: weighted average, clamped to [min,max]. Discrete params
  and all 60 mod-matrix bytes: copied verbatim from the dominant (highest-weight)
  patch -- never blended (same rule as PatchMorpher for the mod matrix).
  Pure logic, header-only.
*/
#pragma once
#include <map>
#include <array>
#include <vector>
#include <cmath>
#include <JuceHeader.h>
#include "TimbreTypes.h"
#include "XpanderData.h"
#include "PatchRandomizer.h"   // isContinuous

namespace PatchBlender
{
    struct BlendResult
    {
        std::map<int,int>      params;
        std::array<uint8_t,60> modBytes{};
        juce::String           name;
    };

    inline BlendResult blend(const std::vector<WeightedPatch>& weights,
                             const std::vector<TimbrePatch>& patches)
    {
        BlendResult out;
        if (weights.empty() || patches.empty()) return out;

        // Dominant = highest weight.
        int   domIdx = weights[0].index;
        float domW   = weights[0].weight;
        for (const auto& w : weights) if (w.weight > domW) { domW = w.weight; domIdx = w.index; }
        if (domIdx < 0 || domIdx >= (int)patches.size()) return out;
        const TimbrePatch& dom = patches[(size_t)domIdx];

        // Seed everything (incl. discrete params) from the dominant patch, copy its mod bytes.
        out.params   = dom.params;
        out.modBytes = dom.modBytes;

        // Overwrite continuous params with the weighted average.
        for (const auto& def : XpanderParams)
        {
            if (!PatchRandomizer::isContinuous(def)) continue;
            double acc = 0.0, wsum = 0.0;
            for (const auto& w : weights)
            {
                if (w.index < 0 || w.index >= (int)patches.size()) continue;
                const auto& pm = patches[(size_t)w.index].params;
                auto it = pm.find(def.id);
                int v = (it != pm.end()) ? it->second : def.min;
                acc  += (double)v * (double)w.weight;
                wsum += (double)w.weight;
            }
            int blended = (wsum > 0.0) ? (int)std::lround(acc / wsum) : def.min;
            out.params[def.id] = juce::jlimit(def.min, def.max, blended);
        }

        out.name = ("TS-" + dom.name.trim().substring(0, 5)).toUpperCase();
        return out;
    }
} // namespace PatchBlender

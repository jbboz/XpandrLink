/*
  PatchRandomizer.h
  Smart patch randomizer + nudge for the Oberheim Xpander / Matrix-12 (TASK-0).

  Pure logic (no UI / no MIDI). Operates on the same {paramID -> value} maps that
  PatchCodec produces and on the 60-byte mod-matrix region (bytes 128-187 of the
  196-byte decoded patch). EditorTabComponent packs the result back into a 399-byte
  SysEx and routes it through the normal load + send paths.

  Design mirrors the musically-aware randomizer in the C# reference
  (RandomizerPage.cs, XpanderTone.cs DefineVCOFrequenciesTuning /
  ForceEnv2ModVca2AfterRandomizeMatrix / RandomizeModulationMatrix).
*/
#pragma once
#include <JuceHeader.h>
#include <map>
#include <array>
#include <cmath>
#include "XpanderData.h"

namespace PatchRandomizer
{
    enum Section { VCO1, VCO2, VCF, FMLAG, ENV, LFO, TRACK, RAMP, MOD, NumSections };
    static_assert(static_cast<int>(NumSections) == 9,
                  "Update Config::sectionEnabled size and sectionName[] when adding a Section");

    inline const char* sectionName(int s)
    {
        static const char* names[NumSections] =
            { "VCO1", "VCO2", "VCF", "FM/LAG", "ENV", "LFO", "TRK", "RAMP", "MOD" };
        return (s >= 0 && s < NumSections) ? names[s] : "?";
    }

    struct Config
    {
        bool sectionEnabled[NumSections];
        int  amount        = 35;    // 1..100 — distance-from-current / change probability
        bool musicalSafety = true;  // keep the result audible / playable

        Config() { for (auto& b : sectionEnabled) b = true; }
        bool isOn(int s) const { return s >= 0 && s < NumSections && sectionEnabled[s]; }
    };

    // --- Parameter IDs referenced by the safety pass (match PatchCodec/XpanderData) ---
    namespace ids
    {
        constexpr int VCO1_FREQ = 0,  VCO1_VOL = 3,  VCO1_WAVE = 4;
        constexpr int VCO2_FREQ = 20;
        constexpr int VCF_FREQ  = 40, VCA1_VOL = 43, VCA2_VOL = 44;
        constexpr int ENV1_DELAY = 130, ENV1_ATK = 131, ENV1_DEC = 132,
                      ENV1_SUS = 133, ENV1_REL = 134, ENV1_AMP = 135;
    }

    // Map a parameter ID to its section (NumSections = unknown / skip). Ranges follow
    // the contiguous ID blocks in XpanderData.h / PatchCodec.h.
    inline int sectionForId(int id)
    {
        if (id >= 0   && id <= 8)   return VCO1;
        if (id >= 20  && id <= 29)  return VCO2;
        if (id >= 40  && id <= 48)  return VCF;
        if (id >= 60  && id <= 66)  return FMLAG;
        if (id >= 70  && id <= 116) return LFO;
        if (id >= 130 && id <= 223) return ENV;
        if (id >= 230 && id <= 255) return TRACK;
        if (id >= 260 && id <= 295) return RAMP;
        return NumSections;
    }

    // --- helpers --------------------------------------------------------------
    inline int currentOr(const std::map<int,int>& m, int id, int fallback)
    {
        auto it = m.find(id);
        return (it != m.end() && it->second != -999) ? it->second : fallback;
    }

    inline bool isContinuous(const XpanderParam& d)
    {
        return d.choices.isEmpty() && !d.isButton && !d.isBitmask && !d.isRadioLED;
    }

    // Ensure the patch is audible and (when MOD routings are available) has a real
    // amplitude envelope. Edits params in place; may request an ENV->VCA routing via
    // the returned flag so the caller can inject it into the mod bytes.
    inline void applyMusicalSafety(std::map<int,int>& p, const Config& cfg, juce::Random& rng)
    {
        if (!cfg.musicalSafety) return;

        // VCO1 must produce a waveform (bitmask 0 = silent oscillator).
        if (currentOr(p, ids::VCO1_WAVE, 0) == 0) p[ids::VCO1_WAVE] = 1; // TRI
        // Keep VCO1 contributing to the mix and the filter at least partly open.
        if (currentOr(p, ids::VCO1_VOL, 0)  < 40) p[ids::VCO1_VOL]  = 50;
        if (currentOr(p, ids::VCF_FREQ, 0)  < 40) p[ids::VCF_FREQ]  = 90;

        // Optionally tune VCO2 to a musical interval of VCO1 (units are semitones).
        if (cfg.isOn(VCO2))
        {
            static const int intervals[] = { 0, 5, 7, 12, -12, 3, 4 };
            int iv = intervals[rng.nextInt(juce::numElementsInArray(intervals))];
            p[ids::VCO2_FREQ] = juce::jlimit(0, 63, currentOr(p, ids::VCO1_FREQ, 24) + iv);
        }

        if (cfg.isOn(MOD))
        {
            // ENV1 will be wired to VCA1 (see randomizeModBytes) → give it a usable shape
            // and let the envelope control amplitude (VCA1 base closed).
            if (currentOr(p, ids::ENV1_AMP, 0) < 50) p[ids::ENV1_AMP] = 63;
            int atk = currentOr(p, ids::ENV1_ATK, 0), dec = currentOr(p, ids::ENV1_DEC, 0),
                sus = currentOr(p, ids::ENV1_SUS, 0), rel = currentOr(p, ids::ENV1_REL, 0);
            if (atk == 0 && dec == 0 && sus == 0 && rel == 0)   // silent envelope → Organ-ish
            { p[ids::ENV1_DELAY] = 0; p[ids::ENV1_SUS] = 63; p[ids::ENV1_REL] = 20; }
            p[ids::VCA1_VOL] = 0;     // amplitude comes from the ENV1 -> VCA1 routing
        }
        // else: MOD scope is off, so no ENV->VCA route will be (re)established here.
        // Do NOT force VCA1 open in that case — with no envelope gating it, an open
        // VCA1 is a static level that never decays on note-off (TASK-07: reported as
        // "infinite sustain" / hardware voice lockup after repeated rolls).

        // Each envelope's FREE RUN and LFO-triggered modes cycle the envelope forever,
        // independent of the note gate -- a real Oberheim mode, but one the randomizer
        // must never land on by chance: an envelope like that keeps sounding even after
        // note-off / All Notes Off (TASK-07: reported as "stuck notes" that never stop).
        // Only touch these when ENV is actually being randomized this roll.
        if (cfg.isOn(ENV))
        {
            for (int env = 0; env < 5; ++env)
            {
                int base = 130 + 20 * env;
                p[base + 7]  = 0;   // FREE RUN
                p[base + 11] = 0;   // LFO-triggered
            }
        }

        // Same risk, different module: each Ramp's LFO-triggered mode (RAMP_TRIG_LFO)
        // retriggers the ramp forever off an LFO cycle, independent of the note gate.
        if (cfg.isOn(RAMP))
        {
            for (int ramp = 0; ramp < 4; ++ramp)
            {
                int base = 260 + 10 * ramp;
                p[base + 3] = 0;   // LFO-triggered
            }
        }
    }

    // Full randomize. amount scales both the distance from current (continuous params)
    // and the probability that discrete params change.
    inline std::map<int,int> randomize(const std::map<int,int>& current,
                                       const Config& cfg, juce::Random& rng)
    {
        std::map<int,int> out = current;
        const int amount = juce::jlimit(1, 100, cfg.amount);

        for (const auto& d : XpanderParams)
        {
            int sect = sectionForId(d.id);
            if (sect == NumSections || !cfg.isOn(sect)) continue;

            // VCA1/VCA2 level is a static, unmodulated gain unless an ENV->VCA route
            // exists (only established here when MOD scope is on — see
            // applyMusicalSafety). Randomizing it open with MOD off leaves nothing to
            // gate it, producing a patch that never decays on note-off. Leave it as-is
            // when MOD is off, even though VCA1/VCA2 are nominally in the "VCF" section.
            if ((d.id == ids::VCA1_VOL || d.id == ids::VCA2_VOL) && !cfg.isOn(MOD))
                continue;

            int cur = currentOr(current, d.id, d.min);
            int nv  = cur;

            if (isContinuous(d))
            {
                int target = d.min + rng.nextInt(d.max - d.min + 1);
                nv = cur + (int) std::lround((double)(target - cur) * amount / 100.0);
            }
            else if (rng.nextInt(100) < amount)
            {
                // Discrete: menus, bitmask waveforms, radio LEDs, on/off buttons.
                nv = d.min + rng.nextInt(d.max - d.min + 1);
            }
            out[d.id] = juce::jlimit(d.min, d.max, nv);
        }

        applyMusicalSafety(out, cfg, rng);
        return out;
    }

    // Small perturbation of the current patch — continuous params only, fixed step,
    // discrete/structural params untouched. Stays close to the source sound.
    inline std::map<int,int> nudge(const std::map<int,int>& current,
                                   const Config& cfg, juce::Random& rng)
    {
        std::map<int,int> out = current;
        for (const auto& d : XpanderParams)
        {
            int sect = sectionForId(d.id);
            if (sect == NumSections || !cfg.isOn(sect) || !isContinuous(d)) continue;

            int range = d.max - d.min;
            int k     = juce::jmax(1, (int) std::lround(range * 0.08));   // ±8% of range
            int delta = rng.nextInt(2 * k + 1) - k;
            int cur   = currentOr(current, d.id, d.min);
            out[d.id] = juce::jlimit(d.min, d.max, cur + delta);
        }
        // Nudge is intentionally subtle; no safety pass (the source patch was already valid).
        return out;
    }

    // Regenerate the 60-byte mod-matrix region (20 entries × [source, amount, dest]).
    // Unused slots use the hardware convention source=0x1F, dest=0x3F. Active slots get
    // a random valid source (<=26), dest (<=46) and signed amount — matching the decode
    // rules in ModSummaryPanel. With musicalSafety, slot 0 is forced to ENV1 -> VCA1 so
    // the amp envelope (shaped by applyMusicalSafety) actually drives the VCA.
    inline std::array<uint8_t,60> randomizeModBytes(const std::array<uint8_t,60>& /*current*/,
                                                    const Config& cfg, juce::Random& rng)
    {
        std::array<uint8_t,60> out{};
        const int amount   = juce::jlimit(1, 100, cfg.amount);
        const int nActive  = juce::jlimit(0, 20, (int) std::lround(amount / 100.0 * 12.0));

        for (int i = 0; i < 20; ++i)
        {
            int base = i * 3;
            if (i < nActive)
            {
                int  src   = rng.nextInt(27);            // 0..26 (NONE=27 reserved for unused)
                int  dst   = rng.nextInt(47);            // 0..46
                int  val   = 1 + rng.nextInt(63);        // 1..63 (non-zero == active)
                bool neg   = rng.nextBool();
                bool qtz   = rng.nextInt(100) < 15;      // quantize occasionally
                out[base+0] = (uint8_t) src;
                out[base+1] = (uint8_t)((val & 0x3F) | (neg ? 0x40 : 0) | (qtz ? 0x80 : 0));
                out[base+2] = (uint8_t) dst;
            }
            else
            {
                out[base+0] = 0x1F;  // UNUSED_ENTRY_SOURCE_VALUE
                out[base+1] = 0x00;
                out[base+2] = 0x3F;  // UNUSED_ENTRY_DEST_VALUE
            }
        }

        if (cfg.musicalSafety)
        {
            // Slot 0: ENV1 (source 12) -> VCA1 vol (dest 8), full positive amount.
            out[0] = 12;
            out[1] = (uint8_t)(63 & 0x3F);
            out[2] = 8;
        }
        return out;
    }

} // namespace PatchRandomizer

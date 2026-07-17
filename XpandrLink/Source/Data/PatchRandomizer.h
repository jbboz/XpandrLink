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
        constexpr int ENV1_MODE_RESET = 136, ENV1_MODE_FREERUN = 137, ENV1_MODE_DADR = 138,
                      ENV1_TRIG_MULTI = 139, ENV1_TRIG_EXTRIG = 140, ENV1_TRIG_LFO = 141,
                      ENV1_TRIG_LFOSRC = 142, ENV1_TRIG_GATED = 143;
    }

    // Four envelope archetypes ported from the upstream reference's amplitude-safety
    // pass (XpanderTone.ForceEnv2ModVca2AfterRandomizeMatrix, which does this for
    // ENV2->VCA2 -- we do the equivalent for ENV1->VCA1). Each row is
    // {Delay, Attack, Decay, Sustain, Release, Amp}. Deliberately a FULL, fixed
    // definition rather than a partial floor of whatever the envelope already was:
    // a floor-only approach (this project's first attempt at this fix) can still
    // leave an enormous attack/decay time in place, which reads as "inaudible" within
    // any normal test-hold length even though the envelope isn't technically silent.
    // Percussive/PercussiveWithRelease intentionally have a low or zero sustain --
    // that's a deliberate style choice upstream supports, not a bug to float away.
    enum class VcaEnvStyle { Organ, StringPad, Percussive, PercussiveWithRelease, NumStyles };
    inline const int* vcaEnvArchetype(VcaEnvStyle style)
    {
        static const int organ[6]       = { 0, 63, 63, 63,  0, 63 };
        static const int stringPad[6]   = { 0, 30, 63, 63, 32, 63 };
        static const int percussive[6]  = { 0,  0, 63, 10,  0, 63 };
        static const int percRelease[6] = { 0,  0, 63,  0, 32, 63 };
        switch (style)
        {
            case VcaEnvStyle::Organ:      return organ;
            case VcaEnvStyle::StringPad:  return stringPad;
            case VcaEnvStyle::Percussive: return percussive;
            default:                      return percRelease;
        }
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

        // These audibility floors only apply within the section(s) the user actually
        // opted into randomizing — matching how VCO2/MOD/ENV/RAMP are already scoped
        // below. Applying them unconditionally meant a roll with every section
        // switched off (or only an unrelated section on, e.g. LFO alone) still forced
        // VCO1's waveform/volume and the VCF cutoff, changing parameters the user never
        // asked to touch.
        if (cfg.isOn(VCO1))
        {
            // VCO1 must produce a waveform (bitmask 0 = silent oscillator).
            if (currentOr(p, ids::VCO1_WAVE, 0) == 0) p[ids::VCO1_WAVE] = 1; // TRI
            // Keep VCO1 contributing to the mix.
            if (currentOr(p, ids::VCO1_VOL, 0)  < 40) p[ids::VCO1_VOL]  = 50;
        }
        if (cfg.isOn(VCF))
        {
            // Keep the filter at least partly open.
            if (currentOr(p, ids::VCF_FREQ, 0)  < 40) p[ids::VCF_FREQ]  = 90;
        }

        // Optionally tune VCO2 to a musical interval of VCO1 (units are semitones).
        if (cfg.isOn(VCO2))
        {
            static const int intervals[] = { 0, 5, 7, 12, -12, 3, 4 };
            int iv = intervals[rng.nextInt(juce::numElementsInArray(intervals))];
            p[ids::VCO2_FREQ] = juce::jlimit(0, 63, currentOr(p, ids::VCO1_FREQ, 24) + iv);
        }

        if (cfg.isOn(MOD))
        {
            // MOD's whole audibility guarantee below (ENV1 forced to a real archetype,
            // VCA1 zeroed so only the envelope gates it) is worthless if a stage further
            // upstream is fully closed -- the oscillator must be producing a waveform at
            // an audible volume and the filter must be at least partly open, or there is
            // no signal for VCA1 to gate in the first place. The VCO1/VCF floors above
            // only fire when cfg.isOn(VCO1)/cfg.isOn(VCF), and MOD-only rolls (the
            // standard test case for this whole audibility saga) leave both off, so MOD
            // needs its own copy of the same floor. (Found 2026-07-12, fix attempt 4:
            // three earlier fixes all targeted ENV1/mod-matrix mechanics and missed that a
            // closed filter or muted oscillator upstream of VCA1 silences a patch no
            // matter how correct the envelope/routing is.)
            if (currentOr(p, ids::VCO1_WAVE, 0) == 0) p[ids::VCO1_WAVE] = 1; // TRI
            if (currentOr(p, ids::VCO1_VOL, 0)  < 40) p[ids::VCO1_VOL]  = 50;
            if (currentOr(p, ids::VCF_FREQ, 0)  < 40) p[ids::VCF_FREQ]  = 90;

            // ENV1 becomes the SOLE amplitude source via the forced ENV1->VCA1 route
            // (randomizeModBytes) -- VCA1_VOL itself is zeroed below so a stuck/self-
            // cycling envelope can never leave a static, gate-independent drone (TASK-07).
            // Ported from the upstream reference's ForceEnv2ModVca2AfterRandomizeMatrix:
            // fully overwrite ENV1's shape with one of the fixed archetypes above (picked
            // at random for variety) rather than floor/adjust whatever the previously-
            // loaded patch (or, if ENV scope is ALSO on this roll, the main per-param
            // randomize loop) left ENV1 at. Two earlier attempts at a partial floor both
            // failed to reliably fix "randomize MOD gives inaudible patches" -- a floor on
            // sustain/delay alone can still leave an enormous, un-capped attack or decay
            // time in place, which reads as silence within any normal test-hold length even
            // though the envelope isn't technically zero. A full, known-good archetype
            // removes that whole class of leftover-state bug.
            auto style = static_cast<VcaEnvStyle>(rng.nextInt(static_cast<int>(VcaEnvStyle::NumStyles)));
            const int* env = vcaEnvArchetype(style);
            p[ids::ENV1_DELAY] = env[0];
            p[ids::ENV1_ATK]   = env[1];
            p[ids::ENV1_DEC]   = env[2];
            p[ids::ENV1_SUS]   = env[3];
            p[ids::ENV1_REL]   = env[4];
            p[ids::ENV1_AMP]   = env[5];

            // Disable every trigger/mode flag so ENV1 behaves as a plain, note-gated
            // envelope -- matches upstream's unconditional reset of the same 8 flags
            // (RESET/FREERUN/DADR/TRIG_MULTI/TRIG_EXTRIG/TRIG_LFO/TRIG_LFOSRC/TRIG_GATED)
            // regardless of whether ENV scope is separately enabled this roll.
            p[ids::ENV1_MODE_RESET]   = 0;
            p[ids::ENV1_MODE_FREERUN] = 0;
            p[ids::ENV1_MODE_DADR]    = 0;
            p[ids::ENV1_TRIG_MULTI]   = 0;   // Single, not Multi
            p[ids::ENV1_TRIG_EXTRIG]  = 0;
            p[ids::ENV1_TRIG_LFO]     = 0;
            p[ids::ENV1_TRIG_LFOSRC]  = 0;
            p[ids::ENV1_TRIG_GATED]   = 0;

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

    // Destinations the musicalSafety amplitude path depends on: VCA1/VCA2 vol (the gain
    // stage) and ENV1's own five mod-matrix destinations (Dly/Atk/Dcy/Rel/Amp) -- a
    // *modulated* stage time or amp on ENV1 can move it away from the raw shape
    // applyMusicalSafety just set, same as a direct level modulation on VCA1/VCA2 can
    // cancel out slot 0's forced ENV1->VCA1 route. See ModDestinations in XpanderData.h
    // for the index order this matches (VCA1_VOL=8, VCA2_VOL=9, Env1 Dly..Amp=20..24).
    inline bool isAmplitudeSafetyDestination(int dest)
    {
        return dest == 8 || dest == 9 || (dest >= 20 && dest <= 24);
    }

    // Regenerate the 60-byte mod-matrix region (20 entries × [source, amount, dest]).
    // Unused slots use the hardware convention source=0x1F, dest=0x3F. Active slots get
    // a random valid source (<=26), dest (<=46) and signed amount — matching the decode
    // rules in ModSummaryPanel. With musicalSafety, slot 0 is forced to ENV1 -> VCA1 so
    // the amp envelope (shaped by applyMusicalSafety) actually drives the VCA.
    //
    // Hardware allows at most MAX_MODULATION_SOURCE (6) simultaneous sources routed to
    // the same destination — confirmed against the reference GetNextAvailableModIDSourceForDest,
    // whose RandomizeModulationMatrix re-rolls the destination until a free per-destination
    // slot exists. A dump with more than 6 entries on one destination has nowhere for the
    // firmware's internal per-destination source table to put the extras, and has been
    // observed to lock up real hardware. Mirror that re-roll here.
    //
    // musicalSafety also excludes the destinations above from every OTHER (non-forced)
    // active entry's random draw. Without this, one of the up-to-11 other random entries
    // could independently land on VCA1_VOL (or an ENV1 stage) with an arbitrary, possibly
    // strongly negative amount -- fighting the deliberately-installed ENV1->VCA1 safety
    // route and producing an inaudible result even though that route itself is intact and
    // correctly encoded. (Found 2026-07-12: an ENV1-shape-only fix did not resolve
    // "randomize MOD gives inaudible patches" because this was a second, independent cause
    // in the same safety pass.)
    inline std::array<uint8_t,60> randomizeModBytes(const std::array<uint8_t,60>& /*current*/,
                                                    const Config& cfg, juce::Random& rng)
    {
        std::array<uint8_t,60> out{};
        const int amount   = juce::jlimit(1, 100, cfg.amount);
        const int nActive  = juce::jlimit(0, 20, (int) std::lround(amount / 100.0 * 12.0));

        static constexpr int kMaxSourcesPerDest = 6;
        std::array<int, 47> destCount{};
        if (cfg.musicalSafety)
            destCount[8] = 1;   // reserve VCA1 vol for the forced ENV1->VCA1 slot 0 below

        for (int i = 0; i < 20; ++i)
        {
            int base = i * 3;
            if (i < nActive)
            {
                int dst = 0;
                for (int attempt = 0; attempt < 100; ++attempt)
                {
                    dst = rng.nextInt(47);
                    if (destCount[(size_t) dst] >= kMaxSourcesPerDest) continue;
                    if (cfg.musicalSafety && isAmplitudeSafetyDestination(dst)) continue;
                    break;
                }
                ++destCount[(size_t) dst];

                int  src   = rng.nextInt(27);            // 0..26 (NONE=27 reserved for unused)
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

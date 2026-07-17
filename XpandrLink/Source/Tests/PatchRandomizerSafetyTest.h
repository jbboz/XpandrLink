/*
  PatchRandomizerSafetyTest.h
  Regression test for an "infinite sustain / hardware lockup" bug:
  randomizing with only VCF in scope (MOD off) produced patches where VCA1
  was forced to a static open level with no envelope routed to it, so the
  amplitude never decayed on note-off (user-confirmed symptom: "held note,
  no decay").

  Verifies: NFR-HW-04 (see docs/NFR.md).
*/
#pragma once
#include <JuceHeader.h>
#include "../Data/PatchRandomizer.h"

class PatchRandomizerSafetyTest : public juce::UnitTest
{
public:
    PatchRandomizerSafetyTest() : juce::UnitTest("PatchRandomizer Safety", "PatchRandomizer") {}

    void runTest() override
    {
        beginTest("VCF-only randomize (MOD off) does not force VCA1 open with no envelope to gate it");
        {
            std::map<int,int> current;
            current[PatchRandomizer::ids::VCA1_VOL] = 0;   // silent, as in a freshly-loaded INIT patch

            PatchRandomizer::Config cfg;
            for (auto& b : cfg.sectionEnabled) b = false;
            cfg.sectionEnabled[PatchRandomizer::VCF] = true;
            cfg.musicalSafety = true;
            cfg.amount = 0;   // amount=0 means the per-param randomize pass is a no-op,
                              // isolating whatever applyMusicalSafety does on its own.

            juce::Random rng(1);
            auto out = PatchRandomizer::randomize(current, cfg, rng);

            // Forcing VCA1 open here would create a static, unmodulated drone that never
            // decays on note-off — the exact bug reported. It must stay untouched.
            expectEquals(out[PatchRandomizer::ids::VCA1_VOL], 0);
        }

        beginTest("VCF-only randomize (MOD off) never opens VCA1/VCA2 via the normal per-param pass either");
        {
            // VCA1_VOL/VCA2_VOL are continuous params grouped in the "VCF" section
            // (XpanderData.h ids 43/44), so the main randomize loop touches them whenever
            // VCF scope is on -- independent of the applyMusicalSafety fallback tested
            // above. Without an ENV->VCA route (MOD off), any nonzero level they land on
            // is a static, unmodulated gain that never decays on note-off. Run many seeds
            // and amounts to make sure none of them open the VCA.
            for (int amount : { 1, 10, 35, 50, 100 })
            {
                for (int seed = 0; seed < 25; ++seed)
                {
                    std::map<int,int> current;
                    current[PatchRandomizer::ids::VCA1_VOL] = 0;
                    current[PatchRandomizer::ids::VCA2_VOL] = 0;

                    PatchRandomizer::Config cfg;
                    for (auto& b : cfg.sectionEnabled) b = false;
                    cfg.sectionEnabled[PatchRandomizer::VCF] = true;
                    cfg.musicalSafety = true;
                    cfg.amount = amount;

                    juce::Random rng(seed);
                    auto out = PatchRandomizer::randomize(current, cfg, rng);

                    expectEquals(out[PatchRandomizer::ids::VCA1_VOL], 0);
                    expectEquals(out[PatchRandomizer::ids::VCA2_VOL], 0);
                }
            }
        }

        beginTest("ENV randomize never leaves FREE RUN or LFO-trigger mode on (gate-independent, never decays)");
        {
            // Each envelope has a FREE RUN button and an LFO-triggered button (XpanderData.h,
            // ids 130+20*env+7 and 130+20*env+11 for env=0..4). Both make the envelope cycle
            // forever independent of the note gate -- a real Oberheim mode, but not one the
            // randomizer should be able to land on by chance: with MOD off there's no route
            // to silence it, and even All Notes Off doesn't stop an envelope that isn't gated
            // by the note in the first place. Sweep amounts/seeds at max probability to make
            // sure musicalSafety always clears both flags after an ENV roll.
            for (int amount : { 1, 35, 100 })
            {
                for (int seed = 0; seed < 40; ++seed)
                {
                    std::map<int,int> current;

                    PatchRandomizer::Config cfg;
                    for (auto& b : cfg.sectionEnabled) b = false;
                    cfg.sectionEnabled[PatchRandomizer::ENV] = true;
                    cfg.musicalSafety = true;
                    cfg.amount = amount;

                    juce::Random rng(seed);
                    auto out = PatchRandomizer::randomize(current, cfg, rng);

                    for (int env = 0; env < 5; ++env)
                    {
                        int base = 130 + 20 * env;
                        expectEquals(out[base + 7], 0);    // FREE RUN
                        expectEquals(out[base + 11], 0);   // LFO-triggered
                    }
                }
            }
        }

        beginTest("RAMP randomize never leaves LFO-trigger mode on (same self-cycling risk as ENV)");
        {
            // Each ramp has an LFO-triggered button (RAMP_TRIG_LFO, ids 260+10*ramp+3 for
            // ramp=0..3), structurally identical to ENV_TRIG_LFO: it retriggers the ramp
            // forever off an LFO cycle, independent of the note gate. Never previously
            // guarded -- this section had no safety pass at all.
            for (int amount : { 1, 35, 100 })
            {
                for (int seed = 0; seed < 40; ++seed)
                {
                    std::map<int,int> current;

                    PatchRandomizer::Config cfg;
                    for (auto& b : cfg.sectionEnabled) b = false;
                    cfg.sectionEnabled[PatchRandomizer::RAMP] = true;
                    cfg.musicalSafety = true;
                    cfg.amount = amount;

                    juce::Random rng(seed);
                    auto out = PatchRandomizer::randomize(current, cfg, rng);

                    for (int ramp = 0; ramp < 4; ++ramp)
                    {
                        int base = 260 + 10 * ramp;
                        expectEquals(out[base + 3], 0);   // LFO-triggered
                    }
                }
            }
        }

        beginTest("Randomize with no sections enabled touches nothing, even with musicalSafety on");
        {
            // applyMusicalSafety's audibility floors (VCO1 waveform/volume, VCF cutoff) used
            // to run unconditionally regardless of section scope, so a roll with every
            // section switched off still silently forced VCF cutoff to 90 (user-reported).
            // They must only fire within a section the user actually opted into.
            std::map<int,int> current;
            current[PatchRandomizer::ids::VCF_FREQ]  = 0;
            current[PatchRandomizer::ids::VCO1_VOL]  = 0;
            current[PatchRandomizer::ids::VCO1_WAVE] = 0;

            PatchRandomizer::Config cfg;
            for (auto& b : cfg.sectionEnabled) b = false;
            cfg.musicalSafety = true;
            cfg.amount = 100;

            juce::Random rng(7);
            auto out = PatchRandomizer::randomize(current, cfg, rng);

            expectEquals(out[PatchRandomizer::ids::VCF_FREQ],  0);
            expectEquals(out[PatchRandomizer::ids::VCO1_VOL],  0);
            expectEquals(out[PatchRandomizer::ids::VCO1_WAVE], 0);
        }

        beginTest("MOD randomize fully overwrites ENV1 with a known-good archetype, regardless of source patch shape");
        {
            // Reported twice: "randomize MOD makes the patch inaudible, even after
            // removing the mod routings." Root cause was never the routing -- ENV1
            // becomes the sole amplitude source via the forced ENV1->VCA1 route. Two
            // earlier fix attempts (float sustain, cap delay) both failed because a
            // *partial* floor can still leave an enormous, un-capped attack/decay time
            // in place -- reads as silence within any normal test-hold length. Ported
            // from the upstream reference (ForceEnv2ModVca2AfterRandomizeMatrix): ENV1's
            // full six-value shape must always come out matching exactly one of the four
            // fixed archetypes, regardless of whatever shape the source patch had --
            // Organ/StringPad/Percussive/PercussiveWithRelease intentionally include low
            // or zero sustain (that's a deliberate style, not a bug), but attack/decay/
            // delay are never left at an arbitrary or excessive value.
            std::map<int,int> current;
            current[PatchRandomizer::ids::ENV1_DELAY] = 0;
            current[PatchRandomizer::ids::ENV1_ATK]   = 10;
            current[PatchRandomizer::ids::ENV1_DEC]   = 40;
            current[PatchRandomizer::ids::ENV1_SUS]   = 0;    // decay-only shape
            current[PatchRandomizer::ids::ENV1_REL]   = 20;

            PatchRandomizer::Config cfg;
            for (auto& b : cfg.sectionEnabled) b = false;
            cfg.sectionEnabled[PatchRandomizer::MOD] = true;
            cfg.musicalSafety = true;
            cfg.amount = 35;

            for (int seed = 0; seed < 40; ++seed)
            {
                juce::Random rng(seed);
                auto out = PatchRandomizer::randomize(current, cfg, rng);

                bool matchesAnArchetype = false;
                for (int s = 0; s < (int) PatchRandomizer::VcaEnvStyle::NumStyles; ++s)
                {
                    const int* env = PatchRandomizer::vcaEnvArchetype((PatchRandomizer::VcaEnvStyle) s);
                    if (out[PatchRandomizer::ids::ENV1_DELAY] == env[0]
                        && out[PatchRandomizer::ids::ENV1_ATK] == env[1]
                        && out[PatchRandomizer::ids::ENV1_DEC] == env[2]
                        && out[PatchRandomizer::ids::ENV1_SUS] == env[3]
                        && out[PatchRandomizer::ids::ENV1_REL] == env[4]
                        && out[PatchRandomizer::ids::ENV1_AMP] == env[5])
                    { matchesAnArchetype = true; break; }
                }
                expect(matchesAnArchetype, "ENV1 shape must exactly match one of the fixed archetypes (seed="
                                           + juce::String(seed) + ")");
                expectEquals(out[PatchRandomizer::ids::ENV1_MODE_FREERUN], 0);
                expectEquals(out[PatchRandomizer::ids::ENV1_TRIG_LFO], 0);
                expectEquals(out[PatchRandomizer::ids::VCA1_VOL], 0);
            }
        }

        beginTest("Mod-matrix randomize never lets another entry cancel the ENV1->VCA1 safety route");
        {
            // Reported again after the ENV1-shape fix above: "randomizing mod continues to
            // give inaudible patches." Root cause #2 in the same safety pass -- slot 0's
            // forced ENV1(src12)->VCA1(dest8) route was correctly encoded, but nothing
            // stopped one of the OTHER (independently randomized) active entries from also
            // landing on VCA1_VOL, VCA2_VOL, or one of ENV1's own mod destinations
            // (Dly/Atk/Dcy/Rel/Amp) with an arbitrary, possibly strongly negative amount --
            // fighting the deliberately-installed safety route. Sweep amounts/seeds at the
            // highest active-entry count to confirm none of the OTHER 19 slots ever target
            // a protected destination when musicalSafety is on.
            for (int amount : { 35, 100 })
            {
                for (int seed = 0; seed < 60; ++seed)
                {
                    PatchRandomizer::Config cfg;
                    for (auto& b : cfg.sectionEnabled) b = false;
                    cfg.sectionEnabled[PatchRandomizer::MOD] = true;
                    cfg.musicalSafety = true;
                    cfg.amount = amount;

                    juce::Random rng(seed);
                    std::array<uint8_t,60> current{};
                    auto mod = PatchRandomizer::randomizeModBytes(current, cfg, rng);

                    // Slot 0 is the forced safety route itself -- only check slots 1..19.
                    for (int i = 1; i < 20; ++i)
                    {
                        int dst = mod[(size_t)(i * 3 + 2)];
                        expect(!PatchRandomizer::isAmplitudeSafetyDestination(dst),
                               "slot " + juce::String(i) + " landed on protected dest "
                               + juce::String(dst) + " (amount=" + juce::String(amount)
                               + " seed=" + juce::String(seed) + ")");
                    }
                }
            }
        }

        beginTest("MOD-only randomize guarantees an open signal path upstream of VCA1, not just ENV1/VCA1 themselves");
        {
            // Reported a third time after the ENV1-archetype and mod-matrix-entry-exclusion
            // fixes above: "randomizing mod still results in silent patches." Root cause #3
            // in the same safety pass -- a SEPARATE, earlier fix (elsewhere in this file,
            // "Randomize with no sections enabled touches nothing") correctly scoped the
            // VCO1-waveform/volume and VCF-cutoff audibility floors behind cfg.isOn(VCO1)/
            // cfg.isOn(VCF), but MOD-only rolls (the standard test case for this entire saga)
            // leave both of those off -- so a source patch with a silent oscillator or a
            // closed filter stays that way, and a perfect ENV1->VCA1 route has no signal to
            // gate. MOD's own audibility guarantee needs its own copy of this floor, the same
            // way it already forces ENV1's shape and VCA1's level open for the same reason.
            std::map<int,int> current;
            current[PatchRandomizer::ids::VCO1_WAVE] = 0;   // silent oscillator
            current[PatchRandomizer::ids::VCO1_VOL]  = 0;   // muted
            current[PatchRandomizer::ids::VCF_FREQ]  = 0;   // fully closed (LP)

            PatchRandomizer::Config cfg;
            for (auto& b : cfg.sectionEnabled) b = false;
            cfg.sectionEnabled[PatchRandomizer::MOD] = true;   // ONLY MOD -- VCO1/VCF stay off
            cfg.musicalSafety = true;
            cfg.amount = 35;

            juce::Random rng(3);
            auto out = PatchRandomizer::randomize(current, cfg, rng);

            expect(out[PatchRandomizer::ids::VCO1_WAVE] != 0, "VCO1 must produce a waveform");
            expect(out[PatchRandomizer::ids::VCO1_VOL]  >= 40, "VCO1 volume must be audible");
            expect(out[PatchRandomizer::ids::VCF_FREQ]  >= 40, "VCF must be at least partly open");
        }

        beginTest("Mod-matrix randomize never routes more than 6 sources to the same destination");
        {
            // Hardware (XpanderTone.ModulationMatrix.cs GetNextAvailableModIDSourceForDest)
            // caps concurrent sources per destination at MAX_MODULATION_SOURCE=6 -- a dump
            // with more than 6 entries on one destination has been observed to lock up real
            // hardware. Sweep amounts/seeds at the highest active-entry count (amount=100 ->
            // 12 active slots) to make sure no destination is ever over-subscribed.
            for (int amount : { 35, 100 })
            {
                for (int seed = 0; seed < 60; ++seed)
                {
                    PatchRandomizer::Config cfg;
                    for (auto& b : cfg.sectionEnabled) b = false;
                    cfg.sectionEnabled[PatchRandomizer::MOD] = true;
                    cfg.musicalSafety = true;
                    cfg.amount = amount;

                    juce::Random rng(seed);
                    std::array<uint8_t,60> current{};
                    auto mod = PatchRandomizer::randomizeModBytes(current, cfg, rng);

                    std::array<int, 47> destCount{};
                    for (int i = 0; i < 20; ++i)
                    {
                        int dst = mod[(size_t)(i * 3 + 2)];
                        if (dst >= 0 && dst < 47) ++destCount[(size_t) dst];
                    }
                    for (int d = 0; d < 47; ++d)
                        expect(destCount[(size_t) d] <= 6,
                               "destination " + juce::String(d) + " exceeded 6 sources"
                               " (amount=" + juce::String(amount) + " seed=" + juce::String(seed) + ")");
                }
            }
        }
    }
};

static PatchRandomizerSafetyTest patchRandomizerSafetyTestInstance;

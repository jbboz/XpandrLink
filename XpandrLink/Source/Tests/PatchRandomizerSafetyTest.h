/*
  PatchRandomizerSafetyTest.h
  Regression test for an "infinite sustain / hardware lockup" bug:
  randomizing with only VCF in scope (MOD off) produced patches where VCA1
  was forced to a static open level with no envelope routed to it, so the
  amplitude never decayed on note-off (user-confirmed symptom: "held note,
  no decay").
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
    }
};

static PatchRandomizerSafetyTest patchRandomizerSafetyTestInstance;

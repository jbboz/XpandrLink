/*
  PatchBlenderTest.h
  Verifies weighted continuous blend + clamp, and dominant-patch copy for
  discrete params and mod bytes.
  Verifies: NFR-HW-11 (Timbre Space blend copies all 60 mod bytes + every
  discrete param from the dominant patch, never blended) — see docs/NFR.md.
*/
#pragma once
#include <JuceHeader.h>
#include "../Data/PatchBlender.h"
#include "../Data/PatchRandomizer.h"

class PatchBlenderTest : public juce::UnitTest
{
public:
    PatchBlenderTest() : juce::UnitTest("PatchBlender", "TimbreSpace") {}

    // First continuous param id and first discrete-with-choices param id, discovered from data.
    static int firstContinuousId()
    {
        for (const auto& def : XpanderParams) if (PatchRandomizer::isContinuous(def)) return def.id;
        return -1;
    }
    static const XpanderParam* firstDiscreteDef()
    {
        for (const auto& def : XpanderParams) if (!PatchRandomizer::isContinuous(def)) return &def;
        return nullptr;
    }

    void runTest() override
    {
        const int contId = firstContinuousId();
        const XpanderParam* disc = firstDiscreteDef();

        beginTest("single patch at weight 1 reproduces its params, mod bytes and name stem");
        {
            TimbrePatch p; p.libraryId = 0; p.name = "ORGAN";
            for (const auto& def : XpanderParams) p.params[def.id] = def.min + 1 <= def.max ? def.min + 1 : def.min;
            for (int i = 0; i < 60; ++i) p.modBytes[(size_t)i] = (uint8_t)(i + 1);
            auto out = PatchBlender::blend({ { 0, 1.0f } }, { p });
            for (const auto& def : XpanderParams)
                expectEquals(out.params[def.id], p.params[def.id]);
            for (int i = 0; i < 60; ++i) expectEquals((int)out.modBytes[(size_t)i], (int)p.modBytes[(size_t)i]);
            expect(out.name.startsWith("TS-"));
        }

        beginTest("50/50 blend averages continuous params and clamps to range");
        {
            TimbrePatch a; a.libraryId = 0; a.name = "AAAA";
            TimbrePatch b; b.libraryId = 1; b.name = "BBBB";
            const XpanderParam* cdef = nullptr;
            for (const auto& def : XpanderParams) if (def.id == contId) cdef = &def;
            a.params[contId] = cdef->min;
            b.params[contId] = cdef->max;
            auto out = PatchBlender::blend({ { 0, 0.5f }, { 1, 0.5f } }, { a, b });
            int mid = (cdef->min + cdef->max) / 2;
            expectWithinAbsoluteError((float)out.params[contId], (float)mid, 1.0f);
            expect(out.params[contId] >= cdef->min && out.params[contId] <= cdef->max);
        }

        beginTest("discrete params and mod bytes come from the dominant (highest-weight) patch");
        {
            TimbrePatch a; a.libraryId = 0; a.name = "AAAA";  // dominant
            TimbrePatch b; b.libraryId = 1; b.name = "BBBB";
            if (disc != nullptr)
            {
                a.params[disc->id] = disc->min;
                b.params[disc->id] = disc->max;
            }
            for (int i = 0; i < 60; ++i) { a.modBytes[(size_t)i] = 0x11; b.modBytes[(size_t)i] = 0x22; }
            auto out = PatchBlender::blend({ { 0, 0.7f }, { 1, 0.3f } }, { a, b });
            if (disc != nullptr) expectEquals(out.params[disc->id], disc->min);   // from A
            for (int i = 0; i < 60; ++i) expectEquals((int)out.modBytes[(size_t)i], 0x11); // from A
        }
    }
};

static PatchBlenderTest patchBlenderTestInstance;

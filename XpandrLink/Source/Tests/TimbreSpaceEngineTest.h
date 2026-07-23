/*
  TimbreSpaceEngineTest.h
  Verifies the deterministic PCA layout and (Task 3) the interpolation query.
  (Layout determinism is a plain correctness property, asserted directly below;
  it has no formal NFR ID — the project's NFR scheme is Timing/Thread/Hardware only.)
*/
#pragma once
#include <JuceHeader.h>
#include "../Data/TimbreSpaceEngine.h"
#include "../Data/PatchRandomizer.h"

class TimbreSpaceEngineTest : public juce::UnitTest
{
public:
    TimbreSpaceEngineTest() : juce::UnitTest("TimbreSpaceEngine", "TimbreSpace") {}

    // Build a patch whose continuous params are all set to `base` (+ per-id offset*k).
    static TimbrePatch makePatch(int id, int base, int k = 0)
    {
        TimbrePatch p;
        p.libraryId = id;
        p.name = "P" + juce::String(id);
        for (const auto& def : XpanderParams)
            if (PatchRandomizer::isContinuous(def))
                p.params[def.id] = juce::jlimit(def.min, def.max, base + k);
        return p;
    }

    void runTest() override
    {
        beginTest("computeLayout normalizes points into [0,1] and keeps one point per non-empty patch");
        {
            std::vector<TimbrePatch> patches;
            for (int i = 0; i < 8; ++i) patches.push_back(makePatch(i, 10 + i * 7, i));
            TimbreSpaceEngine e;
            e.computeLayout(patches);
            expect(!e.isEmpty());
            expectEquals((int)e.getPoints().size(), 8);
            for (const auto& pt : e.getPoints())
            {
                expect(pt.x >= 0.0f && pt.x <= 1.0f);
                expect(pt.y >= 0.0f && pt.y <= 1.0f);
                expect(pt.r >= 0.0f && pt.r <= 1.0f);
                expect(pt.g >= 0.0f && pt.g <= 1.0f);
                expect(pt.b >= 0.0f && pt.b <= 1.0f);
            }
        }

        beginTest("computeLayout is deterministic (same input -> same output)");
        {
            std::vector<TimbrePatch> patches;
            for (int i = 0; i < 6; ++i) patches.push_back(makePatch(i, 5 + i * 11, i * 2));
            TimbreSpaceEngine a, b;
            a.computeLayout(patches);
            b.computeLayout(patches);
            for (size_t i = 0; i < a.getPoints().size(); ++i)
            {
                expectWithinAbsoluteError(a.getPoints()[i].x, b.getPoints()[i].x, 1.0e-6f);
                expectWithinAbsoluteError(a.getPoints()[i].y, b.getPoints()[i].y, 1.0e-6f);
            }
        }

        beginTest("empty (all-zero) patches are filtered out");
        {
            std::vector<TimbrePatch> patches;
            patches.push_back(makePatch(0, 20, 0));
            TimbrePatch empty; empty.libraryId = 1; // no params -> sum 0
            patches.push_back(empty);
            patches.push_back(makePatch(2, 40, 0));
            TimbreSpaceEngine e;
            e.computeLayout(patches);
            expectEquals((int)e.getPoints().size(), 2);
            for (const auto& pt : e.getPoints()) expect(pt.libraryId != 1);
        }

        beginTest("two identical patches project to the same point");
        {
            std::vector<TimbrePatch> patches;
            patches.push_back(makePatch(0, 30, 0));
            patches.push_back(makePatch(1, 30, 0)); // identical params
            patches.push_back(makePatch(2, 80, 5)); // a different one so PCA has variance
            TimbreSpaceEngine e;
            e.computeLayout(patches);
            expectWithinAbsoluteError(e.getPoints()[0].x, e.getPoints()[1].x, 1.0e-4f);
            expectWithinAbsoluteError(e.getPoints()[0].y, e.getPoints()[1].y, 1.0e-4f);
        }

        beginTest("query exactly on a patch point returns that patch at weight ~1");
        {
            std::vector<TimbrePatch> patches;
            for (int i = 0; i < 5; ++i) patches.push_back(makePatch(i, 10 + i * 13, i * 3));
            TimbreSpaceEngine e;
            e.computeLayout(patches);
            const auto& pt = e.getPoints()[2];
            auto w = e.interpolationDataForPoint(pt.x, pt.y);
            expect(!w.empty());
            // The dominant weight must be point index 2, ~1.0.
            int dom = w[0].index; float dw = w[0].weight;
            for (auto& x : w) if (x.weight > dw) { dw = x.weight; dom = x.index; }
            expectEquals(dom, 2);
            expect(dw > 0.98f);
        }

        beginTest("interpolation weights are non-negative and sum to 1");
        {
            std::vector<TimbrePatch> patches;
            for (int i = 0; i < 7; ++i) patches.push_back(makePatch(i, 8 + i * 9, i * 4));
            TimbreSpaceEngine e;
            e.computeLayout(patches);
            auto w = e.interpolationDataForPoint(0.5f, 0.5f);
            expect(!w.empty());
            expect((int)w.size() <= 3);
            float sum = 0.0f;
            for (auto& x : w) { expect(x.weight >= 0.0f); sum += x.weight; }
            expectWithinAbsoluteError(sum, 1.0f, 1.0e-4f);
        }

        beginTest("query far outside the hull still returns a valid normalized weight set");
        {
            std::vector<TimbrePatch> patches;
            for (int i = 0; i < 4; ++i) patches.push_back(makePatch(i, 12 + i * 15, i * 5));
            TimbreSpaceEngine e;
            e.computeLayout(patches);
            auto w = e.interpolationDataForPoint(5.0f, 5.0f);   // way outside [0,1]^2
            expect(!w.empty());
            float sum = 0.0f;
            for (auto& x : w) { expect(x.weight >= 0.0f); sum += x.weight; }
            expectWithinAbsoluteError(sum, 1.0f, 1.0e-4f);
        }
    }
};

static TimbreSpaceEngineTest timbreSpaceEngineTestInstance;

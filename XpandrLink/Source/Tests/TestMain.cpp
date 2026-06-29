#include <JuceHeader.h>
#include "BUG01_KnobColDecodeTest.h"
#include "BUG05_ProgramChangeSysexTest.h"
#include "IDSourceTrackingTest.h"
#include "InitPatchLoadTest.h"
#include "PatchRandomizerSafetyTest.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult(i))
            failures += r->failures;

    return failures > 0 ? 1 : 0;
}

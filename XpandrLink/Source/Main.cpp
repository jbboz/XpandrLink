#include <JuceHeader.h>
#include "Plugin/XpandrLinkAudioProcessor.h"

// Build with -DXPANDRLINK_RUN_TESTS=1 (e.g. via a Debug test scheme) to include
// and auto-register the unit tests with juce::UnitTestRunner.
#ifdef XPANDRLINK_RUN_TESTS
#include "Tests/BUG01_KnobColDecodeTest.h"
#include "Tests/BUG05_ProgramChangeSysexTest.h"
#include "Tests/IDSourceTrackingTest.h"
#endif

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XpandrLinkAudioProcessor();
}

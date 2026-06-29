#include "XpandrLinkAudioProcessor.h"
#include "XpandrLinkAudioProcessorEditor.h"
#include "../UI/ThemeData.h"
#include "../Data/OberheimDefs.h"

// In standalone builds, the Options > Audio/MIDI Settings dialog uses the
// StandalonePluginHolder's AudioDeviceManager. We share it with MidiEngine so
// that MIDI inputs configured there immediately affect XpandrLink's routing.
#if JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

XpandrLinkAudioProcessor::XpandrLinkAudioProcessor()
    // TASK-26: this constructor compiles into the shared code, where the
    // JucePlugin_Build_* macros are all 1 — so the standalone/plugin split MUST be a
    // runtime check, not #if. Standalone (a MIDI editor) declares no audio buses, so
    // the StandalonePluginHolder opens 0 channels and never grabs an audio device.
    // AU/VST3 keep stereo buses — this is what keeps the AU type aufx.
    : AudioProcessor (juce::JUCEApplicationBase::isStandaloneApp()
                        ? BusesProperties()
                        : BusesProperties()
                            .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      paramBridge (*this)
{
    // Register as a listener on every AU parameter so parameterValueChanged fires
    // when the host drives automation.
    for (auto* p : getParameters())
        p->addListener (this);

    // Register as a MidiEngine listener so hardware knob movements update AU params.
    midiEngine.addListener (this);

#if JucePlugin_Build_Standalone
    // Share the standalone app's AudioDeviceManager so Options > Audio/MIDI Settings
    // controls XpandrLink's actual MIDI routing (inputs shown as checkboxes).
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        midiEngine.useDeviceManager (holder->deviceManager);
#endif

#if JUCE_DEBUG
    juce::UnitTestRunner runner;
    runner.runAllTests();
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult(i);
        DBG("TEST [" << result->unitTestName << " / " << result->subcategoryName << "] "
            << result->passes << " passed, " << result->failures << " failed.");
        for (const auto& msg : result->messages)
            DBG("  " << msg);
    }
#endif
}

XpandrLinkAudioProcessor::~XpandrLinkAudioProcessor()
{
    midiEngine.removeListener (this);

    for (auto* p : getParameters())
        p->removeListener (this);

    // Clear all typeface references before static cleanup runs.
    // CoreTextTypeface destructs via ObjC; if those run during __cxa_finalize
    // after the ObjC runtime tears down, std::terminate() is called.
    // ThemeData holds a static Typeface::Ptr for the DSEG14 font that is NOT
    // in the JUCE typeface cache, so it must be cleared explicitly first.
    ThemeData::clearCachedFonts();
    juce::Typeface::clearTypefaceCache();
}

juce::AudioProcessorEditor* XpandrLinkAudioProcessor::createEditor()
{
    return new XpandrLinkAudioProcessorEditor (*this);
}

//==============================================================================
// UI knob/button → host automation notification
//
// Called (on the message thread) right before a UI-initiated sendParameterToSynth
// actually sends. We call setValueNotifyingHost so the AU host (Ableton, Logic)
// sees the parameter move and can record it as automation.
// hardwareUpdateMode is set as the re-entry guard so parameterValueChanged doesn't
// immediately re-dispatch the same value back to the synth.
void XpandrLinkAudioProcessor::onParameterSentToSynth (int page, int paramCol,
                                                        int value, bool /*isButton*/)
{
    // If this send was triggered by host automation playback, the host already
    // knows the value — notifying it again looks like a manual override to
    // Ableton and disables the automation lane.
    if (fromHostAutomation)
        return;

    int idx = paramBridge.findParamIndex (page, paramCol);
    if (idx < 0) return;

    const auto* entry = paramBridge.getEntry (idx);
    if (entry == nullptr) return;

    auto& params = getParameters();
    if (idx >= (int)params.size()) return;

    float normalized = paramBridge.normalize (*entry, value);

    midiEngine.setHardwareUpdateMode (true);
    params[idx]->setValueNotifyingHost (normalized);
    midiEngine.setHardwareUpdateMode (false);
}

//==============================================================================
// Host automation → hardware
//
// Called on the audio thread (or message thread) whenever the host changes a
// parameter value. We dispatch to the message thread before calling
// sendParameterToSynth because that function uses Timer::callAfterDelay.
void XpandrLinkAudioProcessor::parameterValueChanged (int parameterIndex, float newValue)
{
    if (midiEngine.isInHardwareUpdateMode())
        return;

    const auto* entry = paramBridge.getEntry (parameterIndex);
    if (entry == nullptr) return;

    int  hwValue = paramBridge.denormalize (*entry, newValue);
    int  page    = entry->page;
    int  col     = entry->paramCol;
    bool isBtn   = entry->isButton;

    juce::WeakReference<XpandrLinkAudioProcessor> weakThis (this);
    juce::WeakReference<MidiEngine> weakEngine (&midiEngine);
    juce::MessageManager::callAsync ([weakThis, weakEngine, page, col, hwValue, isBtn]
    {
        if (weakThis == nullptr || weakEngine == nullptr) return;

        // Send to synth (fromHostAutomation suppresses the re-notify back to host)
        weakThis->fromHostAutomation = true;
        weakEngine->sendParameterToSynth (page, col, hwValue, isBtn);
        weakThis->fromHostAutomation = false;

        // Push the new value to the editor UI via the same path that hardware uses.
        // updatingUIFromHost stops onParameterChangedFromHardware from calling
        // setValueNotifyingHost (the host already knows — it sent us the value).
        weakThis->updatingUIFromHost = true;
        weakEngine->broadcastParameterChange (page, col, hwValue, false);
        weakThis->updatingUIFromHost = false;
    });
}

//==============================================================================
// Hardware → host automation
//
// Called on the MIDI thread. Dispatches to the message thread, sets the guard
// flag around setValueNotifyingHost so our own parameterValueChanged doesn't
// echo the value back to the synth.
void XpandrLinkAudioProcessor::onParameterChangedFromHardware (int page, int paramCol,
                                                               int value, bool isDelta, bool /*isButton*/)
{
    // Triggered by our own broadcastParameterChange — host already set this value.
    if (updatingUIFromHost) return;

    int idx = paramBridge.findParamIndex (page, paramCol);
    if (idx < 0) return;

    const auto* entry = paramBridge.getEntry (idx);
    if (entry == nullptr) return;

    auto& params = getParameters();
    if (idx >= params.size()) return;
    auto* param = params[idx];

    juce::WeakReference<XpandrLinkAudioProcessor> weakThis (this);
    juce::WeakReference<MidiEngine> weakEngine (&midiEngine);

    XpandrLinkParameterBridge::ParamEntry captured = *entry;

    juce::MessageManager::callAsync ([weakThis, weakEngine, param, captured, value, isDelta]
    {
        if (weakThis == nullptr || weakEngine == nullptr) return;

        float normalized;
        if (isDelta)
        {
            float current  = param->getValue();
            int   range    = captured.maxVal - captured.minVal;
            int   currentHw = captured.minVal + (int)std::round (current * (float)range);
            int   newHw     = juce::jlimit (captured.minVal, captured.maxVal, currentHw + value);
            normalized = (range == 0) ? 0.0f : (float)(newHw - captured.minVal) / (float)range;
        }
        else
        {
            int range  = captured.maxVal - captured.minVal;
            normalized = (range == 0) ? 0.0f
                       : juce::jlimit (0.0f, 1.0f,
                             (float)(value - captured.minVal) / (float)range);
        }

        weakEngine->setHardwareUpdateMode (true);
        param->setValueNotifyingHost (normalized);
        weakEngine->setHardwareUpdateMode (false);
    });
}

//==============================================================================
void XpandrLinkAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = juce::XmlElement ("XpandrLinkState");

    // MIDI configuration — save all enabled inputs as comma-separated
    state.setAttribute ("midiInput",  midiEngine.getEnabledMidiInputNames().joinIntoString (","));
    state.setAttribute ("midiOutput", midiEngine.getCurrentMidiOutputName());

    // Cached patch SysEx — serialised so the host can re-send it to hardware on project load.
    // Force cache[5] to the scratchpad slot so restore always lands at slot 99, matching
    // the redirect that sendPatchToSynth applies. The original program number is kept in
    // the library file; it is irrelevant in a DAW session context.
    auto cachedPatch = midiEngine.getCachedPatch();
    if (!cachedPatch.empty())
    {
        if (cachedPatch.size() == 399)
            cachedPatch[5] = (uint8_t)Oberheim::kScratchpadProgram;
        juce::MemoryBlock mb (cachedPatch.data(), cachedPatch.size());
        state.setAttribute ("cachedPatch", mb.toBase64Encoding());
    }

    // All AU parameter values (normalized 0..1 stored as double for precision)
    auto* paramsEl = state.createNewChildElement ("Params");
    const auto& params = getParameters();
    for (int i = 0; i < params.size(); ++i)
        paramsEl->setAttribute ("p" + juce::String (i), (double)params[i]->getValue());

    juce::AudioProcessor::copyXmlToBinary (state, destData);
}

void XpandrLinkAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || xml->getTagName() != "XpandrLinkState")
        return;

    // Restore MIDI config on the message thread (MidiEngine is not thread-safe for setup calls)
    juce::String inputName  = xml->getStringAttribute ("midiInput");
    juce::String outputName = xml->getStringAttribute ("midiOutput");
    juce::String patchB64   = xml->getStringAttribute ("cachedPatch");

    // Set the patch cache synchronously so it is available immediately if the editor
    // opens before the callAsync block below has a chance to run (race otherwise).
    if (patchB64.isNotEmpty())
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (patchB64) && mb.getSize() == 399)
        {
            std::vector<uint8_t> bytes (static_cast<const uint8_t*> (mb.getData()),
                                        static_cast<const uint8_t*> (mb.getData()) + mb.getSize());
            midiEngine.setCachedPatch (bytes);
        }
    }

    juce::WeakReference<MidiEngine> weakEngine (&midiEngine);
    juce::MessageManager::callAsync ([weakEngine, inputName, outputName, patchB64]
    {
        if (weakEngine == nullptr) return;
        // Restore all saved inputs (comma-separated; single name is backward-compatible)
        if (inputName.isNotEmpty())
        {
            auto inputList = juce::StringArray::fromTokens (inputName, ",", "");
            for (auto& name : inputList)
                if (name.trim().isNotEmpty()) weakEngine->addMidiInput (name.trim());
        }

        if (outputName.isNotEmpty()) weakEngine->setMidiOutput (outputName);

        // Broadcast cached patch to any already-open editor UI, then schedule retries
        // to send the patch to hardware — the first retry that finds the output open
        // sends once and stops.
        if (patchB64.isNotEmpty())
        {
            weakEngine->broadcastCachedPatch();

            auto sent = std::make_shared<bool> (false);
            for (int delayMs : { 2000, 5000, 10000, 20000 })
                juce::Timer::callAfterDelay (delayMs, [weakEngine, outputName, sent]()
                {
                    if (weakEngine == nullptr || *sent) return;
                    if (!weakEngine->isMidiOutputOpen() && outputName.isNotEmpty())
                        weakEngine->setMidiOutput (outputName);
                    if (weakEngine->isMidiOutputOpen())
                    {
                        *sent = true;
                        weakEngine->sendPatchToSynth();
                    }
                });
        }
    });

    // Restore parameter values — guard the hardware update mode so none of the
    // setValueNotifyingHost calls dispatch SysEx to the synth during restore.
    const auto* paramsEl = xml->getChildByName ("Params");
    if (paramsEl == nullptr) return;

    auto& params = getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        juce::String key = "p" + juce::String (i);
        if (paramsEl->hasAttribute (key))
        {
            float val = (float)paramsEl->getDoubleAttribute (key);
            midiEngine.setHardwareUpdateMode (true);
            params[i]->setValueNotifyingHost (val);
            midiEngine.setHardwareUpdateMode (false);
        }
    }
}

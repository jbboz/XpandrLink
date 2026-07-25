/*
  PatchOrchestrator.h
  Extracted from EditorTabComponent (Phase 4 / TASK-13b step 3). Owns the "assemble a
  399-byte patch from live params + mod bytes + name, then push it through the normal
  load/send pipeline" pattern shared by the randomizer and the one-level undo snapshot.

  Live UI state (current param values, current mod-matrix bytes, current patch name) is
  read through injected callbacks -- EditorTabComponent still owns voiceArch/advancedPanel/
  fullModMatrix/currentPatchName -- so this class has zero juce::Component dependencies and
  is fully unit-testable (see PatchOrchestratorTest.h).
*/
#pragma once
#include <JuceHeader.h>
#include <functional>
#include <map>
#include <array>
#include <vector>
#include <cstdint>
#include "../Engine/MidiEngine.h"

class PatchOrchestrator
{
public:
    using ParamMapFn  = std::function<std::map<int,int>()>;
    using ModBytesFn  = std::function<std::array<uint8_t,60>()>;
    using PatchNameFn = std::function<juce::String()>;

    PatchOrchestrator(MidiEngine& engine,
                      ParamMapFn getParamMap,
                      ModBytesFn getModBytes,
                      PatchNameFn getPatchName);

    // Pack params + a 60-byte mod region + name into a 399-byte single-patch SysEx
    // (edit-buffer program 0). modBytes may be nullptr (mod region left zero).
    std::vector<uint8_t> encodePatchSysex(const std::map<int,int>& params,
                                          const uint8_t* modBytes,
                                          const juce::String& name) const;

    // Convenience: encode the CURRENT live state via the injected callbacks.
    std::vector<uint8_t> buildCurrentPatchSysex() const;

    // Cache + broadcast (drives UI via onPatchReceived) + All-Notes-Off + send to hardware
    // scratchpad slot 99 (BUG-32) -- the shared "apply a rolled/nudged patch" pipeline.
    void applyPatch(const std::map<int,int>& params, const std::array<uint8_t,60>& modBytes);

    // One-level undo, used by the randomizer's Revert button.
    void snapshotForRevert();
    bool canRevert() const { return revertSysex_.size() == 399; }
    void revert();

private:
    MidiEngine& midiEngine_;
    ParamMapFn  getParamMap_;
    ModBytesFn  getModBytes_;
    PatchNameFn getPatchName_;
    std::vector<uint8_t> revertSysex_;   // one-level undo snapshot (pre-roll patch)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchOrchestrator)
};

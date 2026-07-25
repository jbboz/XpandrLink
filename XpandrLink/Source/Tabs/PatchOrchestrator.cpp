#include "PatchOrchestrator.h"
#include "../Data/PatchCodec.h"

PatchOrchestrator::PatchOrchestrator(MidiEngine& engine,
                                     ParamMapFn getParamMap,
                                     ModBytesFn getModBytes,
                                     PatchNameFn getPatchName)
    : midiEngine_(engine)
    , getParamMap_(std::move(getParamMap))
    , getModBytes_(std::move(getModBytes))
    , getPatchName_(std::move(getPatchName))
{
}

std::vector<uint8_t> PatchOrchestrator::encodePatchSysex(const std::map<int,int>& params,
                                                         const uint8_t* modBytes,
                                                         const juce::String& nameIn) const
{
    std::vector<uint8_t> raw = PatchCodec::encode(params);  // 196 bytes (mod region = 0)

    // Mod matrix (bytes 128-187): PatchCodec::encode leaves these zero, so write the
    // caller's 60-byte block (preserved-from-load for saves, randomized for rolls).
    if (modBytes != nullptr)
        for (int i = 0; i < 60; ++i)
            raw[(size_t)(128 + i)] = modBytes[i];

    // Patch name into bytes 188-195 (8 ASCII chars, space-padded)
    {
        juce::String name = nameIn;
        name = (name == "--------") ? "        " : name.substring(0, 8).paddedRight(' ', 8);
        for (int i = 0; i < 8; ++i)
            raw[(size_t)(188 + i)] = (uint8_t)(name[i] & 0x7F);
    }

    // Pack each byte into two 7-bit MIDI bytes, wrap in SysEx header.
    // Format: F0 10 [sysexID] 01 00 00 [392 packed bytes] F7 = 399 bytes total
    std::vector<uint8_t> sysex;
    sysex.reserve(399);
    sysex.push_back(0xF0);
    sysex.push_back(0x10);
    sysex.push_back((uint8_t)midiEngine_.getSysexID());
    sysex.push_back(0x01);  // single patch dump command
    sysex.push_back(0x00);
    sysex.push_back(0x00);  // program 0 = edit buffer
    for (uint8_t b : raw)
    {
        sysex.push_back(b & 0x7F);          // lo: bits 0-6
        sysex.push_back((b >> 7) & 0x01);   // hi: bit 7
    }
    sysex.push_back(0xF7);

    jassert(sysex.size() == 399);
    return sysex;
}

std::vector<uint8_t> PatchOrchestrator::buildCurrentPatchSysex() const
{
    auto mod = getModBytes_();
    return encodePatchSysex(getParamMap_(), mod.data(), getPatchName_());
}

void PatchOrchestrator::applyPatch(const std::map<int,int>& params,
                                   const std::array<uint8_t,60>& modBytes)
{
    auto sysex = encodePatchSysex(params, modBytes.data(), getPatchName_());
    midiEngine_.setCachedPatch(sysex);   // 399-byte cache
    midiEngine_.broadcastCachedPatch();  // drives UI via onPatchReceived (params + mod summary + name)
    // TASK-07: swapping patches on the synth while a note is held/sustaining can leave
    // that voice's gate orphaned on this hardware (reported as "stuck notes" after
    // repeated randomize rolls). Silence everything right before the new patch lands.
    midiEngine_.sendAllNotesOff();
    midiEngine_.sendPatchToSynth();      // -> hardware scratchpad slot 99 (BUG-32)
}

void PatchOrchestrator::snapshotForRevert()
{
    revertSysex_ = buildCurrentPatchSysex();
}

void PatchOrchestrator::revert()
{
    if (revertSysex_.size() != 399) return;
    midiEngine_.setCachedPatch(revertSysex_);
    midiEngine_.broadcastCachedPatch();
    midiEngine_.sendPatchToSynth();
    revertSysex_.clear();
}

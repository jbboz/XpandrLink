/*
  PatchSysexFile.h
  Sysex-file-format utilities extracted from PatchLibrary (Phase 4 / P1 decomposition).
  Pure, stateless, no SQLite/DB dependency -- everything here operates only on raw bytes
  and juce::File, which is what makes it directly unit-testable without a live PatchLibrary.
*/
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cstdint>

namespace PatchSysexFile
{
    // True if b[i..i+398] looks like a valid 399-byte single-patch dump
    // (F0 10 .. 01 00 .. .. F7 at the right offsets). Caller guarantees i+399 <= b.size().
    bool isPatch399Block(const std::vector<uint8_t>& b, size_t i);

    // FNV-1a 64-bit hash of a file's raw bytes. Returns 0 on failure (e.g. file missing).
    uint64_t hashFile(const juce::File& f);

    // Number of valid 399-byte single-patch blocks found in a file (0 if none/invalid).
    int countPatchesInFile(const juce::File& file);

    // Extract every valid 399-byte single-patch block from a bank (AllDataDump) file.
    std::vector<std::vector<uint8_t>> extractPatchesFromBank(const juce::File& bankFile);

    // Extract the 8-char patch name from a 399-byte SysEx blob (bytes 188-195, packed).
    juce::String extractNameFromSysex(const std::vector<uint8_t>& sysex399);

    // Write an 8-char name into a 399-byte SysEx blob's name bytes, in place.
    // Uppercased, space-padded to 8 chars. No-op if sysex399 isn't 399 bytes.
    void embedNameIntoSysex(std::vector<uint8_t>& sysex399, const juce::String& name);

    // Copy src into destDir, appending _N on a same-name/different-content collision.
    // Returns the destination file, or an empty File on failure.
    juce::File safeCopyToDir(const juce::File& src, const juce::File& destDir,
                             const juce::String& preferredStem = {});
}

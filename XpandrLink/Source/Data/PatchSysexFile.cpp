#include "PatchSysexFile.h"

namespace PatchSysexFile
{

bool isPatch399Block(const std::vector<uint8_t>& b, size_t i)
{
    return b[i] == 0xF0 && b[i+1] == 0x10 &&
           b[i+3] == 0x01 && b[i+4] == 0x00 && b[i+398] == 0xF7;
}

// FNV-1a 64-bit hash of a file's raw bytes. Returns 0 on failure.
uint64_t hashFile(const juce::File& f)
{
    juce::FileInputStream fis(f);
    if (!fis.openedOk()) return 0;
    uint64_t h = 14695981039346656037ULL;
    while (!fis.isExhausted())
    {
        h ^= (uint64_t)(uint8_t)fis.readByte();
        h *= 1099511628211ULL;
    }
    return h;
}

int countPatchesInFile(const juce::File& file)
{
    if (!file.existsAsFile() || file.getSize() < 399) return 0;
    juce::FileInputStream fis(file);
    if (!fis.openedOk()) return 0;
    std::vector<uint8_t> bytes((size_t)file.getSize());
    fis.read(bytes.data(), (int)bytes.size());

    int count = 0;
    for (size_t i = 0; i + 399 <= bytes.size(); )
    {
        if (isPatch399Block(bytes, i))
        { ++count; i += 399; }
        else ++i;
    }
    return count;
}

juce::String extractNameFromSysex(const std::vector<uint8_t>& sysex399)
{
    if (sysex399.size() < 399) return {};
    // SysEx header is 6 bytes; patch name is at logical bytes 188-195 of the 196-byte payload.
    // Each logical byte is packed as two 7-bit bytes: lo = sysex[6+2*i], hi = sysex[6+2*i+1].
    juce::String name;
    for (int i = 0; i < 8; ++i)
    {
        int offset = 6 + 2 * (188 + i);
        uint8_t lo = sysex399[(size_t)offset]     & 0x7F;
        uint8_t hi = sysex399[(size_t)(offset + 1)] & 0x01;
        char c = (char)((hi << 7) | lo);
        if (c == 0) break;
        name += c;
    }
    return name.trim().toUpperCase();
}

void embedNameIntoSysex(std::vector<uint8_t>& sysex399, const juce::String& name)
{
    if (sysex399.size() < 399) return;
    juce::String n = name.trim().toUpperCase().substring(0, 8).paddedRight(' ', 8);
    for (int i = 0; i < 8; ++i)
    {
        uint8_t b = (uint8_t)(n[i] & 0x7F);
        int offset = 6 + 2 * (188 + i);            // packed: lo at offset, hi at offset+1
        sysex399[(size_t)offset]       = (uint8_t)(b & 0x7F);
        sysex399[(size_t)(offset + 1)] = (uint8_t)((b >> 7) & 0x01);
    }
}

std::vector<std::vector<uint8_t>> extractPatchesFromBank(const juce::File& bankFile)
{
    juce::FileInputStream fis(bankFile);
    if (!fis.openedOk()) return {};

    auto fileSize = (size_t)bankFile.getSize();
    std::vector<uint8_t> bytes(fileSize);
    fis.read(bytes.data(), (int)fileSize);

    std::vector<std::vector<uint8_t>> patches;
    size_t i = 0;
    while (i + 399 <= fileSize)
    {
        if (isPatch399Block(bytes, i))
        {
            patches.push_back(std::vector<uint8_t>(
                bytes.begin() + static_cast<ptrdiff_t>(i),
                bytes.begin() + static_cast<ptrdiff_t>(i) + 399));
            i += 399;
        }
        else { ++i; }
    }
    return patches;
}

juce::File safeCopyToDir(const juce::File& src, const juce::File& destDir,
                         const juce::String& preferredStem)
{
    if (!src.existsAsFile()) return {};
    juce::String stem = preferredStem.isNotEmpty()
        ? preferredStem.replace(" ", "_").replace("/", "_")
        : src.getFileNameWithoutExtension();

    juce::File dest = destDir.getChildFile(stem + ".syx");
    if (!dest.exists())
    {
        return src.copyFileTo(dest) ? dest : juce::File{};
    }
    // Same content already present? Compare bytes, not size -- every single-patch
    // file is exactly 399 bytes, so size equality alone would treat any name
    // collision as identical content and silently attach the wrong file.
    if (hashFile(dest) == hashFile(src)) return dest;

    for (int n = 2; n <= 99; ++n)
    {
        dest = destDir.getChildFile(stem + "_" + juce::String(n) + ".syx");
        if (!dest.exists())
            return src.copyFileTo(dest) ? dest : juce::File{};
    }
    return {};
}

} // namespace PatchSysexFile

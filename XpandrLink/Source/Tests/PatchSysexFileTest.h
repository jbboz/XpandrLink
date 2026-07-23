/*
  PatchSysexFileTest.h
  Unit tests for PatchSysexFile (Phase 4 / P1 decomposition): the sysex-file-format
  utilities extracted from PatchLibrary. Pure byte/file operations, no SQLite -- built here
  from synthetic 399-byte blocks rather than real captured patches.
*/
#pragma once
#include <JuceHeader.h>
#include "../Data/PatchSysexFile.h"

class PatchSysexFileTest : public juce::UnitTest
{
public:
    PatchSysexFileTest() : juce::UnitTest("PatchSysexFile", "Data") {}

    static std::vector<uint8_t> makeMinimalPatchBlock()
    {
        std::vector<uint8_t> b(399, 0);
        b[0] = 0xF0; b[1] = 0x10; b[3] = 0x01; b[4] = 0x00; b[398] = 0xF7;
        return b;
    }

    void runTest() override
    {
        beginTest("isPatch399Block accepts a valid block and rejects a malformed one");
        {
            auto block = makeMinimalPatchBlock();
            expect(PatchSysexFile::isPatch399Block(block, 0), "well-formed block must be accepted");

            auto broken = block;
            broken[398] = 0x00; // wrong terminator
            expect(!PatchSysexFile::isPatch399Block(broken, 0), "wrong terminator byte must be rejected");
        }

        beginTest("hashFile: identical content hashes identically; different content differs; missing file returns 0");
        {
            auto tmpDir = juce::File::createTempFile("patchsysexfile_test_dir");
            tmpDir.deleteFile();
            tmpDir.createDirectory();

            auto fileA = tmpDir.getChildFile("a.syx");
            auto fileB = tmpDir.getChildFile("b.syx"); // same content as A
            auto fileC = tmpDir.getChildFile("c.syx"); // different content
            auto block = makeMinimalPatchBlock();
            auto blockDiff = block; blockDiff[10] = 0x7F;

            fileA.replaceWithData(block.data(), (int)block.size());
            fileB.replaceWithData(block.data(), (int)block.size());
            fileC.replaceWithData(blockDiff.data(), (int)blockDiff.size());

            expect(PatchSysexFile::hashFile(fileA) == PatchSysexFile::hashFile(fileB),
                   "identical bytes must hash identically");
            expect(PatchSysexFile::hashFile(fileA) != PatchSysexFile::hashFile(fileC),
                   "different bytes must hash differently");
            expect(PatchSysexFile::hashFile(tmpDir.getChildFile("missing.syx")) == 0,
                   "a missing file hashes to 0");

            tmpDir.deleteRecursively();
        }

        beginTest("embedNameIntoSysex / extractNameFromSysex round-trip");
        {
            auto block = makeMinimalPatchBlock();
            PatchSysexFile::embedNameIntoSysex(block, "lead pad"); // lowercase + 8 chars exactly
            auto name = PatchSysexFile::extractNameFromSysex(block);
            expectEquals(name, juce::String("LEAD PAD"), "name comes back uppercased, unchanged length");
        }

        beginTest("embedNameIntoSysex pads and truncates to exactly 8 chars");
        {
            auto block = makeMinimalPatchBlock();
            PatchSysexFile::embedNameIntoSysex(block, "hi");
            expectEquals(PatchSysexFile::extractNameFromSysex(block), juce::String("HI"),
                        "extract trims trailing padding back off");
        }

        beginTest("countPatchesInFile / extractPatchesFromBank find every valid block in a multi-patch file");
        {
            auto tmpDir = juce::File::createTempFile("patchsysexfile_test_bank_dir");
            tmpDir.deleteFile();
            tmpDir.createDirectory();
            auto bankFile = tmpDir.getChildFile("bank.syx");

            auto blockA = makeMinimalPatchBlock();
            auto blockB = makeMinimalPatchBlock();
            blockB[20] = 0x33; // distinguish from blockA without breaking validity

            std::vector<uint8_t> bank;
            bank.insert(bank.end(), blockA.begin(), blockA.end());
            bank.insert(bank.end(), blockB.begin(), blockB.end());
            bankFile.replaceWithData(bank.data(), (int)bank.size());

            expectEquals(PatchSysexFile::countPatchesInFile(bankFile), 2, "two back-to-back valid blocks");

            auto extracted = PatchSysexFile::extractPatchesFromBank(bankFile);
            expectEquals((int)extracted.size(), 2, "extracts both blocks");
            expect(extracted[0] == blockA, "first extracted block matches the first source block");
            expect(extracted[1] == blockB, "second extracted block matches the second source block");

            tmpDir.deleteRecursively();
        }

        beginTest("safeCopyToDir: same content dedups to the existing file; different content gets a numbered suffix");
        {
            auto tmpDir = juce::File::createTempFile("patchsysexfile_test_copy_dir");
            tmpDir.deleteFile();
            tmpDir.createDirectory();
            auto destDir = tmpDir.getChildFile("dest");
            destDir.createDirectory();

            auto srcA   = tmpDir.getChildFile("src_a.syx");
            auto blockA = makeMinimalPatchBlock();
            srcA.replaceWithData(blockA.data(), (int)blockA.size());

            auto dest1 = PatchSysexFile::safeCopyToDir(srcA, destDir, "PATCH");
            expect(dest1 != juce::File{}, "first copy succeeds");
            expectEquals(dest1.getFileName(), juce::String("PATCH.syx"), "first copy uses the preferred stem");

            // Re-copying identical content under the same stem must return the SAME dest,
            // not create PATCH_2.syx -- this is the dedup safeCopyToDir exists for.
            auto dest2 = PatchSysexFile::safeCopyToDir(srcA, destDir, "PATCH");
            expectEquals(dest2.getFullPathName(), dest1.getFullPathName(),
                        "identical content under the same stem must dedup to the existing file");

            auto srcC   = tmpDir.getChildFile("src_c.syx");
            auto blockC = blockA; blockC[15] = 0x11;
            srcC.replaceWithData(blockC.data(), (int)blockC.size());
            auto dest3 = PatchSysexFile::safeCopyToDir(srcC, destDir, "PATCH");
            expectEquals(dest3.getFileName(), juce::String("PATCH_2.syx"),
                        "different content under a colliding stem gets a numbered suffix");

            tmpDir.deleteRecursively();
        }
    }
};

static PatchSysexFileTest patchSysexFileTestInstance;

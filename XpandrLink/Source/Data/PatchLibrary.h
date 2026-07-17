/*
  PatchLibrary.h
  SQLite-backed catalog of Xpander/Matrix-12 single-patch .syx files.
*/
#pragma once
#include <JuceHeader.h>
#include <vector>

struct PatchEntry
{
    juce::String id;              // UUID — stable even if file is renamed
    juce::String name;            // 8-char patch name from SysEx bytes 188-195
    juce::String description;     // free-text; defaults to source filename (no .syx)
    juce::File   file;            // absolute path on disk
    bool         favorite     = false;
    bool         isDuplicate  = false; // true if another entry has identical content
    juce::int64  dateAdded    = 0;    // epoch seconds
    juce::String contentHash;         // FNV-1a hex; cached in DB to avoid per-refresh re-read
};

class PatchLibrary
{
public:
    PatchLibrary();
    ~PatchLibrary();

    // Opens (or creates) the database. Call before any other method.
    void open();
    void close();
    bool isOpen() const;

    // Default paths
    static juce::File defaultDbFile();      // AppSupport/XpandrLink/PatchLibrary.db
    static juce::File defaultLibraryRoot(); // ~/Documents/OberheimPatches/Library (user-definable)

    // Library root — all imports are copied here
    void       setLibraryRoot(const juce::File& dir);
    juce::File getLibraryRoot() const { return libraryRoot_; }

    // Import a single .syx file: copy to libraryRoot, add to DB. A non-empty
    // description overrides the default (source filename).
    // Returns new cache index, or -1 if duplicate / invalid.
    int importPatch(const juce::File& sourceFile, const juce::String& description = {});

    // Import a bank (AllDataDump) file: extract all 399-byte single patches,
    // write each to libraryRoot/<bankStem>/<name>_NN.syx, add all to DB. A non-empty
    // description is applied to every extracted patch (default = per-patch filename).
    // Returns number of patches extracted.
    int importBankFile(const juce::File& bankFile, const juce::String& description = {});

    // Unified import: auto-detects single vs. bank by counting 399-byte patch
    // blocks. Returns number of patches imported (0 on failure).
    int importFile(const juce::File& sourceFile, const juce::String& description = {});

    // Number of valid 399-byte single-patch blocks in a file (0 if none) — public so
    // the UI can decide single vs. bank before prompting for a description.
    static int patchCountInFile(const juce::File& file) { return countPatchesInFile(file); }

    // Update a patch's free-text description (write-through to DB).
    void setDescription(int index, const juce::String& description);

    // Write current editor SysEx as a new patch file and DB entry.
    // Returns new cache index, or -1 on failure.
    int saveCurrentPatch(const std::vector<uint8_t>& sysex399, const juce::String& name);

    // Query — indices into internal cache
    int getNumPatches() const         { return (int)cache_.size(); }
    const PatchEntry& getPatch(int i) const { return cache_[(size_t)i]; }

    // Returns cache indices matching query string and filter.
    juce::Array<int> search(const juce::String& query, bool favoritesOnly) const;

    // Write-through mutations
    void toggleFavorite(int index);
    void renamePatch(int index, const juce::String& newName);  // updates DB + renames file
    void removePatch(int index);          // removes DB row; file stays on disk
    void removePatches(const juce::Array<int>& indices); // batch remove by cache index
    void removeDuplicates();             // removes all entries with isDuplicate==true

    // Display sort order for the in-memory cache (search() returns indices in this order).
    enum class SortMode { Name, DateAdded, Description };
    void     setSortMode(SortMode m);
    SortMode getSortMode() const { return sortMode_; }

    // Reload cache from DB (call after external changes)
    void refresh();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    juce::File libraryRoot_;
    std::vector<PatchEntry> cache_;
    SortMode   sortMode_ = SortMode::Name;

    // Reorder cache_ in place per sortMode_. Called at the end of refresh() and by
    // setSortMode(). Duplicate marking always runs on name order first, so the
    // canonical (non-dup) entry is independent of the chosen display sort.
    void applySort();

    // Extract 8-char patch name from a 399-byte SysEx blob
    static juce::String extractNameFromSysex(const std::vector<uint8_t>& sysex399);

    // Write an 8-char name into the patch name bytes (188-195) of a 399-byte SysEx blob,
    // in place. Name is uppercased, space-padded to 8 chars. No-op if sysex isn't 399 bytes.
    static void embedNameIntoSysex(std::vector<uint8_t>& sysex399, const juce::String& name);

    // Scan bankFile for all valid 399-byte single-patch SysEx blocks
    static std::vector<std::vector<uint8_t>> extractPatchesFromBank(const juce::File& bankFile);

    // Copy src into destDir, appending _N suffix on collision.
    // Returns destination file, or empty File on failure.
    static juce::File safeCopyToDir(const juce::File& src,
                                    const juce::File& destDir,
                                    const juce::String& preferredStem = {});

    // Insert a new row; refresh() is NOT called — caller is responsible.
    bool insertRow(const juce::String& id, const juce::String& name,
                   const juce::File& file, const juce::String& description);

    // Count valid 399-byte single-patch blocks in a file (0 if none/invalid).
    static int countPatchesInFile(const juce::File& file);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchLibrary)
};

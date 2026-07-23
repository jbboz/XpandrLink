/*
  PatchLibrary.cpp
  SQLite-backed Xpander/Matrix-12 patch catalog.
*/
#include "PatchLibrary.h"
#include "../../ThirdParty/sqlite/sqlite3.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// Pimpl — keeps sqlite3* out of the public header
// ---------------------------------------------------------------------------
struct PatchLibrary::Impl
{
    sqlite3* db = nullptr;
};

// ---------------------------------------------------------------------------
// Ctor / dtor
// ---------------------------------------------------------------------------
PatchLibrary::PatchLibrary()  : impl_(std::make_unique<Impl>()) {}
PatchLibrary::~PatchLibrary() { close(); }

// ---------------------------------------------------------------------------
// Static paths
// ---------------------------------------------------------------------------
juce::File PatchLibrary::defaultDbFile()
{
#if JUCE_WINDOWS
    // On Windows userApplicationDataDirectory is %APPDATA%; no "Application Support" level.
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("XpandrLink/PatchLibrary.db");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Application Support/XpandrLink/PatchLibrary.db");
#endif
}

juce::File PatchLibrary::defaultLibraryRoot()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
               .getChildFile("OberheimPatches/Library");
}

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------
void PatchLibrary::open()
{
    juce::File dbFile = defaultDbFile();
    dbFile.getParentDirectory().createDirectory();
    libraryRoot_ = defaultLibraryRoot();

    if (sqlite3_open(dbFile.getFullPathName().toRawUTF8(), &impl_->db) != SQLITE_OK)
    {
        juce::Logger::writeToLog("PatchLibrary: cannot open DB: "
                                 + juce::String(sqlite3_errmsg(impl_->db)));
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        return;
    }

    // WAL mode for better concurrency; ignored if already set
    sqlite3_exec(impl_->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS patches ("
        "  id         TEXT PRIMARY KEY,"
        "  name       TEXT NOT NULL,"
        "  file       TEXT NOT NULL,"
        "  favorite   INTEGER NOT NULL DEFAULT 0,"
        "  date_added INTEGER NOT NULL DEFAULT 0,"
        "  tags       TEXT NOT NULL DEFAULT ''"
        ");"
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key   TEXT PRIMARY KEY,"
        "  value TEXT"
        ");";

    char* err = nullptr;
    sqlite3_exec(impl_->db, ddl, nullptr, nullptr, &err);
    if (err) { juce::Logger::writeToLog("PatchLibrary DDL: " + juce::String(err)); sqlite3_free(err); }

    // Migrations: ADD COLUMN is idempotent (SQLite ignores "duplicate column" error).
    sqlite3_exec(impl_->db,
        "ALTER TABLE patches ADD COLUMN description TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);
    sqlite3_exec(impl_->db,
        "ALTER TABLE patches ADD COLUMN content_hash TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);

    // Restore libraryRoot from DB settings
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
            "SELECT value FROM settings WHERE key='libraryRoot';",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            juce::File saved(juce::String::fromUTF8(
                (const char*)sqlite3_column_text(stmt, 0)));
            if (saved.isDirectory()) libraryRoot_ = saved;
        }
        sqlite3_finalize(stmt);
    }

    libraryRoot_.createDirectory();
    refresh();
}

bool PatchLibrary::isOpen() const { return impl_->db != nullptr; }

void PatchLibrary::close()
{
    if (impl_->db)
    {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
    }
}


// ---------------------------------------------------------------------------
// Library root
// ---------------------------------------------------------------------------
void PatchLibrary::setLibraryRoot(const juce::File& dir)
{
    libraryRoot_ = dir;
    libraryRoot_.createDirectory();

    if (!impl_->db) return;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
            "INSERT OR REPLACE INTO settings (key, value) VALUES ('libraryRoot', ?);",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        auto p = dir.getFullPathName();
        sqlite3_bind_text(stmt, 1, p.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// ---------------------------------------------------------------------------
// refresh — rebuild in-memory cache from DB
// ---------------------------------------------------------------------------
void PatchLibrary::refresh()
{
    cache_.clear();
    if (!impl_->db) return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
            "SELECT id, name, file, favorite, date_added, description, content_hash"
            " FROM patches ORDER BY name;",
            -1, &stmt, nullptr) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        PatchEntry e;
        e.id          = juce::String::fromUTF8((const char*)sqlite3_column_text(stmt, 0));
        e.name        = juce::String::fromUTF8((const char*)sqlite3_column_text(stmt, 1));
        e.file        = juce::File(juce::String::fromUTF8((const char*)sqlite3_column_text(stmt, 2)));
        e.favorite    = sqlite3_column_int(stmt, 3) != 0;
        e.dateAdded   = sqlite3_column_int64(stmt, 4);
        if (auto* d = (const char*)sqlite3_column_text(stmt, 5))
            e.description = juce::String::fromUTF8(d);
        if (auto* h = (const char*)sqlite3_column_text(stmt, 6))
            e.contentHash = juce::String::fromUTF8(h);
        cache_.push_back(e);
    }
    sqlite3_finalize(stmt);

    // Mark duplicate entries using the cached content_hash (TASK-19).
    // For rows with no hash yet (legacy entries or new imports before the migration),
    // compute and persist the hash now so subsequent refreshes are free.
    std::map<juce::String, bool> seenHash;
    for (auto& e : cache_)
    {
        e.isDuplicate = false;
        if (!e.file.existsAsFile()) continue;

        if (e.contentHash.isEmpty())
        {
            uint64_t h = PatchSysexFile::hashFile(e.file);
            if (h == 0) continue;
            e.contentHash = juce::String::toHexString((juce::int64)h);

            sqlite3_stmt* upd = nullptr;
            if (sqlite3_prepare_v2(impl_->db,
                    "UPDATE patches SET content_hash=? WHERE id=?;",
                    -1, &upd, nullptr) == SQLITE_OK)
            {
                sqlite3_bind_text(upd, 1, e.contentHash.toRawUTF8(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(upd, 2, e.id.toRawUTF8(),          -1, SQLITE_TRANSIENT);
                sqlite3_step(upd);
                sqlite3_finalize(upd);
            }
        }

        if (seenHash.count(e.contentHash))
            e.isDuplicate = true;
        else
            seenHash[e.contentHash] = true;
    }

    applySort();
}

// ---------------------------------------------------------------------------
// Sorting
// ---------------------------------------------------------------------------
void PatchLibrary::applySort()
{
    switch (sortMode_)
    {
        case SortMode::Name:
            std::stable_sort(cache_.begin(), cache_.end(),
                [](const PatchEntry& a, const PatchEntry& b)
                { return a.name.compareIgnoreCase(b.name) < 0; });
            break;
        case SortMode::DateAdded:   // newest first
            std::stable_sort(cache_.begin(), cache_.end(),
                [](const PatchEntry& a, const PatchEntry& b)
                { return a.dateAdded > b.dateAdded; });
            break;
        case SortMode::Description:
            std::stable_sort(cache_.begin(), cache_.end(),
                [](const PatchEntry& a, const PatchEntry& b)
                { return a.description.compareIgnoreCase(b.description) < 0; });
            break;
    }
}

void PatchLibrary::setSortMode(SortMode m)
{
    sortMode_ = m;
    applySort();
}

// ---------------------------------------------------------------------------
// search
// ---------------------------------------------------------------------------
juce::Array<int> PatchLibrary::search(const juce::String& query, bool favoritesOnly) const
{
    juce::Array<int> result;
    juce::String q = query.toUpperCase().trim();
    for (int i = 0; i < (int)cache_.size(); ++i)
    {
        const auto& e = cache_[(size_t)i];
        if (favoritesOnly && !e.favorite) continue;
        if (q.isNotEmpty()
            && !e.name.toUpperCase().contains(q)
            && !e.file.getParentDirectory().getFileName().toUpperCase().contains(q))
            continue;
        result.add(i);
    }
    return result;
}

// ---------------------------------------------------------------------------
// importPatch — copy single .syx and add to DB
// ---------------------------------------------------------------------------
int PatchLibrary::importPatch(const juce::File& sourceFile, const juce::String& description)
{
    if (!sourceFile.existsAsFile() || sourceFile.getSize() < 399)
        return -1;

    juce::FileInputStream fis(sourceFile);
    if (!fis.openedOk()) return -1;
    std::vector<uint8_t> bytes((size_t)sourceFile.getSize());
    fis.read(bytes.data(), (int)bytes.size());

    // Find first valid 399-byte single-patch block (TASK-12: uses shared helper)
    std::vector<uint8_t> sysex399;
    for (size_t i = 0; i + 399 <= bytes.size(); ++i)
    {
        if (PatchSysexFile::isPatch399Block(bytes, i))
        {
            sysex399.assign(bytes.begin() + static_cast<ptrdiff_t>(i),
                            bytes.begin() + static_cast<ptrdiff_t>(i) + 399);
            break;
        }
    }
    if (sysex399.empty()) return -1;

    juce::String name = PatchSysexFile::extractNameFromSysex(sysex399);
    juce::String stem = name.isEmpty() ? sourceFile.getFileNameWithoutExtension() : name;

    // Check duplicate by filename in destination
    juce::File dest = PatchSysexFile::safeCopyToDir(sourceFile, libraryRoot_, stem);
    if (dest == juce::File{}) return -1;

    // Description: caller-supplied if non-empty, else source filename base (without .syx).
    juce::String desc = description.isNotEmpty() ? description
                                                 : sourceFile.getFileNameWithoutExtension();
    juce::String id = juce::Uuid().toString();
    if (!insertRow(id, name.isEmpty() ? dest.getFileNameWithoutExtension() : name, dest, desc))
        return -1;

    refresh();

    // Find new index by id
    for (int i = 0; i < (int)cache_.size(); ++i)
        if (cache_[(size_t)i].id == id) return i;
    return -1;
}

// ---------------------------------------------------------------------------
// importBankFile — extract all patches from an AllDataDump
// ---------------------------------------------------------------------------
int PatchLibrary::importBankFile(const juce::File& bankFile, const juce::String& description)
{
    auto patches = PatchSysexFile::extractPatchesFromBank(bankFile);
    if (patches.empty()) return 0;

    juce::File subDir = libraryRoot_.getChildFile(bankFile.getFileNameWithoutExtension());
    subDir.createDirectory();

    int count = 0;
    for (int i = 0; i < (int)patches.size(); ++i)
    {
        const auto& blob = patches[(size_t)i];
        juce::String name = PatchSysexFile::extractNameFromSysex(blob);
        juce::String stem = (name.isEmpty() ? "patch" : name)
                            + "_" + juce::String::formatted("%02d", i + 1);

        // Write extracted sysex to file
        juce::File tempFile = juce::File::createTempFile("xpandrlink_import.syx");
        {
            juce::FileOutputStream fos(tempFile);
            if (!fos.openedOk()) continue;
            fos.write(blob.data(), (int)blob.size());
        }

        juce::File dest = PatchSysexFile::safeCopyToDir(tempFile, subDir, stem);
        tempFile.deleteFile();
        if (dest == juce::File{}) continue;

        // Description: caller-supplied (applied to every patch in the bank) if non-empty,
        // else the extracted patch's filename base (e.g. NAME_01).
        juce::String desc = description.isNotEmpty() ? description
                                                     : dest.getFileNameWithoutExtension();
        juce::String id = juce::Uuid().toString();
        if (insertRow(id, name.isEmpty() ? dest.getFileNameWithoutExtension() : name, dest, desc))
            ++count;
    }

    if (count > 0) refresh();
    return count;
}

// ---------------------------------------------------------------------------
// importFile — auto-detect single vs bank
// ---------------------------------------------------------------------------
int PatchLibrary::importFile(const juce::File& sourceFile, const juce::String& description)
{
    int n = PatchSysexFile::countPatchesInFile(sourceFile);
    if (n <= 0) return 0;
    if (n == 1) return importPatch(sourceFile, description) >= 0 ? 1 : 0;
    return importBankFile(sourceFile, description);
}

// ---------------------------------------------------------------------------
// saveCurrentPatch — write editor SysEx to library
// ---------------------------------------------------------------------------
int PatchLibrary::saveCurrentPatch(const std::vector<uint8_t>& sysex399, const juce::String& name)
{
    if (sysex399.size() != 399) return -1;

    juce::String useName = name.trim().toUpperCase();
    if (useName.isEmpty()) useName = "UNTITLED";
    juce::String stem = useName.replace(" ", "_");

    // Embed the chosen name into the patch's SysEx name bytes (188-195) so the saved
    // file carries it — otherwise reloading shows the editor's old embedded name (e.g.
    // a morph's "M-" name) instead of what the user typed in Save As.
    std::vector<uint8_t> outSysex = sysex399;
    PatchSysexFile::embedNameIntoSysex(outSysex, useName);

    // If an entry with this name already exists, overwrite it in-place rather than
    // creating a new DB row. All patches are 399 bytes so safeCopyToDir's size-equality
    // shortcut would always skip the write, leaving the old content on disk and
    // creating a phantom duplicate entry in the DB.
    for (int i = 0; i < (int)cache_.size(); ++i)
    {
        auto& e = cache_[(size_t)i];
        if (e.name != useName || !e.file.existsAsFile()) continue;

        if (!e.file.replaceWithData(outSysex.data(), (size_t)outSysex.size())) return -1;

        juce::String newHash = juce::String::toHexString((juce::int64)PatchSysexFile::hashFile(e.file));
        juce::String savedId = e.id;
        if (impl_->db)
        {
            sqlite3_stmt* upd = nullptr;
            if (sqlite3_prepare_v2(impl_->db,
                    "UPDATE patches SET content_hash=? WHERE id=?;",
                    -1, &upd, nullptr) == SQLITE_OK)
            {
                sqlite3_bind_text(upd, 1, newHash.toRawUTF8(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(upd, 2, savedId.toRawUTF8(), -1, SQLITE_TRANSIENT);
                sqlite3_step(upd);
                sqlite3_finalize(upd);
            }
        }
        refresh();
        for (int j = 0; j < (int)cache_.size(); ++j)
            if (cache_[(size_t)j].id == savedId) return j;
        return -1;
    }

    // No existing entry with this name — create a new file and DB row.
    juce::File tempFile = juce::File::createTempFile("xpandrlink_save.syx");
    {
        juce::FileOutputStream fos(tempFile);
        if (!fos.openedOk()) return -1;
        fos.write(outSysex.data(), (int)outSysex.size());
    }

    juce::File dest = PatchSysexFile::safeCopyToDir(tempFile, libraryRoot_, stem);
    tempFile.deleteFile();
    if (dest == juce::File{}) return -1;

    juce::String id = juce::Uuid().toString();
    if (!insertRow(id, useName, dest, useName)) return -1;

    refresh();
    for (int i = 0; i < (int)cache_.size(); ++i)
        if (cache_[(size_t)i].id == id) return i;
    return -1;
}

// ---------------------------------------------------------------------------
// toggleFavorite
// ---------------------------------------------------------------------------
void PatchLibrary::toggleFavorite(int index)
{
    if (index < 0 || index >= (int)cache_.size() || !impl_->db) return;
    bool newVal = !cache_[(size_t)index].favorite;
    cache_[(size_t)index].favorite = newVal;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
            "UPDATE patches SET favorite=? WHERE id=?;",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, newVal ? 1 : 0);
        auto id = cache_[(size_t)index].id;
        sqlite3_bind_text(stmt, 2, id.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// ---------------------------------------------------------------------------
// renamePatch — update DB name + rename file
// ---------------------------------------------------------------------------
void PatchLibrary::renamePatch(int index, const juce::String& newName)
{
    if (index < 0 || index >= (int)cache_.size() || !impl_->db) return;
    juce::String trimmed = newName.trim().toUpperCase().substring(0, 8);
    if (trimmed.isEmpty()) return;

    auto& entry = cache_[(size_t)index];
    juce::String stem = trimmed.replace(" ", "_");
    juce::File newFile = entry.file.getParentDirectory().getChildFile(stem + ".syx");

    // Avoid stomping another file
    if (newFile != entry.file && newFile.exists())
    {
        for (int i = 2; i <= 99; ++i)
        {
            newFile = entry.file.getParentDirectory()
                          .getChildFile(stem + "_" + juce::String(i) + ".syx");
            if (!newFile.exists()) break;
        }
    }
    if (newFile != entry.file) entry.file.moveFileTo(newFile);

    // Rewrite the embedded SysEx name (188-195) so a reload shows the new name, not the
    // stale one baked into the file at save time.
    {
        juce::MemoryBlock mb;
        if (newFile.loadFileAsData(mb) && mb.getSize() == 399)
        {
            std::vector<uint8_t> sysex((const uint8_t*)mb.getData(),
                                       (const uint8_t*)mb.getData() + mb.getSize());
            PatchSysexFile::embedNameIntoSysex(sysex, trimmed);
            newFile.replaceWithData(sysex.data(), sysex.size());
        }
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
            "UPDATE patches SET name=?, file=? WHERE id=?;",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        auto fp = newFile.getFullPathName();
        sqlite3_bind_text(stmt, 1, trimmed.toRawUTF8(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, fp.toRawUTF8(),       -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, entry.id.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    refresh();
}

// ---------------------------------------------------------------------------
// removePatch — remove from DB; file stays on disk
// ---------------------------------------------------------------------------
void PatchLibrary::removePatch(int index)
{
    if (index < 0 || index >= (int)cache_.size() || !impl_->db) return;
    auto id = cache_[(size_t)index].id;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
            "DELETE FROM patches WHERE id=?;",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, id.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    refresh();
}

// ---------------------------------------------------------------------------
// removePatches — batch remove (multi-select). Resolve indices→ids first so cache
// reindexing during deletion can't invalidate later targets; refresh once at end.
// ---------------------------------------------------------------------------
void PatchLibrary::removePatches(const juce::Array<int>& indices)
{
    if (!impl_->db) return;
    std::vector<juce::String> ids;
    for (int idx : indices)
        if (idx >= 0 && idx < (int)cache_.size())
            ids.push_back(cache_[(size_t)idx].id);

    for (const auto& id : ids)
    {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(impl_->db,
                "DELETE FROM patches WHERE id=?;",
                -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, id.toRawUTF8(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    refresh();
}

// ---------------------------------------------------------------------------
// removeDuplicates — delete all DB rows where isDuplicate == true
// ---------------------------------------------------------------------------
void PatchLibrary::removeDuplicates()
{
    if (!impl_->db) return;
    // Collect IDs of duplicates (stable — unaffected by cache index shifts)
    std::vector<juce::String> ids;
    for (const auto& e : cache_)
        if (e.isDuplicate) ids.push_back(e.id);

    sqlite3_stmt* stmt = nullptr;
    for (const auto& id : ids)
    {
        if (sqlite3_prepare_v2(impl_->db,
                "DELETE FROM patches WHERE id=?;",
                -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, id.toRawUTF8(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    refresh();
}

// ---------------------------------------------------------------------------
// insertRow — private helper
// ---------------------------------------------------------------------------

bool PatchLibrary::insertRow(const juce::String& id, const juce::String& name,
                              const juce::File& file, const juce::String& description)
{
    if (!impl_->db) return false;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR IGNORE INTO patches (id, name, file, favorite, date_added, description, content_hash)"
        " VALUES (?, ?, ?, 0, ?, ?, ?);";
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    auto fp = file.getFullPathName();
    juce::int64 now = juce::Time::getCurrentTime().toMilliseconds() / 1000;
    juce::String hash = juce::String::toHexString((juce::int64)PatchSysexFile::hashFile(file));
    sqlite3_bind_text(stmt, 1, id.toRawUTF8(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.toRawUTF8(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fp.toRawUTF8(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_text(stmt, 5, description.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, hash.toRawUTF8(),        -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

void PatchLibrary::setDescription(int index, const juce::String& description)
{
    if (index < 0 || index >= (int)cache_.size() || !impl_->db) return;
    cache_[(size_t)index].description = description;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
            "UPDATE patches SET description=? WHERE id=?;",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        auto id = cache_[(size_t)index].id;
        sqlite3_bind_text(stmt, 1, description.toRawUTF8(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.toRawUTF8(),          -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}


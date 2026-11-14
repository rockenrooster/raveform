#pragma once
// Crash-safe saves (M7, PLAN: release preparation). Every file the game keeps between runs (settings and hints, high scores,
// the tier cache, the baked audio, the sync calibration) is written whole to `<path>.tmp`, flushed to the disk, and then put
// in the old file's place in one step: rename on POSIX (atomic), MoveFileEx with replace and write-through on Windows. A
// crash or a full disk leaves the old file or the new one, never half of either; a stale .tmp is overwritten by the next save.
// Loading is each reader's own: a missing, short or damaged file loads as defaults.

#include <cstddef>
#include <cstdio>

namespace rf {

// The whole file at once.
bool saveFileAtomic(const char* path, const void* data, size_t length);

// For writers that stream (the baked audio): begin opens `<path>.tmp`; commit flushes, syncs and closes it and, when `ok`,
// renames it over `path` (else removes it). commit returns false on any failure (the old file is kept).
struct SaveFile {
    FILE* f = nullptr;
    char path[600] = {};
    char tmp[608] = {};
};
bool saveBegin(SaveFile& s, const char* path);
bool saveCommit(SaveFile& s, bool ok);

}  // namespace rf

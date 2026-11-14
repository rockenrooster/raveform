#include "core/save_file.h"

#include <cstring>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace rf {

namespace {
bool syncAndClose(FILE* f) {
    bool ok = std::fflush(f) == 0;
#if defined(_WIN32)
    ok = ok && _commit(_fileno(f)) == 0;
#else
    ok = ok && fsync(fileno(f)) == 0;
#endif
    return (std::fclose(f) == 0) && ok;
}

bool replace(const char* tmp, const char* path) {
#if defined(_WIN32)
    return MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return std::rename(tmp, path) == 0;  // atomic: the old file stays until the new one takes its name
#endif
}
}  // namespace

bool saveBegin(SaveFile& s, const char* path) {
    s.f = nullptr;
    const size_t n = std::strlen(path);
    if (n + 1 > sizeof s.path) return false;
    std::memcpy(s.path, path, n + 1);
    std::memcpy(s.tmp, path, n);
    std::memcpy(s.tmp + n, ".tmp", 5);
    s.f = std::fopen(s.tmp, "wb");
    return s.f != nullptr;
}

bool saveCommit(SaveFile& s, bool ok) {
    if (!s.f) return false;
    ok = syncAndClose(s.f) && ok;
    s.f = nullptr;
    if (!ok) {
        std::remove(s.tmp);
        return false;
    }
    if (!replace(s.tmp, s.path)) {
        std::remove(s.tmp);
        return false;
    }
    return true;
}

bool saveFileAtomic(const char* path, const void* data, size_t length) {
    SaveFile s;
    if (!saveBegin(s, path)) return false;
    const bool wrote = length == 0 || std::fwrite(data, 1, length, s.f) == length;
    return saveCommit(s, wrote);
}

}  // namespace rf

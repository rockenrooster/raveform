// M7 (PLAN, release preparation): crash-safe saves. Every file the game keeps between runs is written whole through a temporary
// file and a rename (core/save_file), and a missing, short or damaged one loads as defaults without a crash: settings.txt (the
// settings and the first-run hints), highscores.txt, the tier cache and the baked audio. Files go to %TEMP% (or TMPDIR).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "audio/bake.h"
#include "audio/music_data.h"
#include "core/save_file.h"
#include "game/settings.h"
#include "game/tiers.h"

using namespace rf;
using namespace rf::audio;

namespace {
int g_checks = 0, g_failures = 0;
#define CHECK(c)                                                                     \
    do {                                                                             \
        ++g_checks;                                                                  \
        if (!(c)) {                                                                  \
            ++g_failures;                                                            \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);               \
        }                                                                            \
    } while (0)

char g_dir[400];

void pathOf(char* out, size_t cap, const char* name) { std::snprintf(out, cap, "%s/raveform_save_test_%s", g_dir, name); }

bool exists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

long sizeOf(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return -1;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fclose(f);
    return n;
}

void writeRaw(const char* path, const void* data, size_t n) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    std::fwrite(data, 1, n, f);
    std::fclose(f);
}

// The first `keep` bytes of a file (a save cut short by a crash or a full disk, as an older writer could leave it).
void truncateTo(const char* path, long keep) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return;
    static char buf[1 << 16];
    const size_t n = std::fread(buf, 1, sizeof buf, f);
    std::fclose(f);
    writeRaw(path, buf, static_cast<size_t>(keep) < n ? static_cast<size_t>(keep) : n);
}

void garbage(const char* path, unsigned seed, size_t n) {
    static unsigned char buf[4096];
    for (size_t i = 0; i < n && i < sizeof buf; ++i) {
        seed = seed * 1664525u + 1013904223u;
        buf[i] = static_cast<unsigned char>(seed >> 24);
    }
    writeRaw(path, buf, n < sizeof buf ? n : sizeof buf);
}

void testSaveFile() {
    std::printf("[save tests] the atomic write\n");
    char p[512], tmp[520];
    pathOf(p, sizeof p, "atomic.txt");
    std::snprintf(tmp, sizeof tmp, "%s.tmp", p);
    std::remove(p);
    CHECK(saveFileAtomic(p, "one\n", 4) && sizeOf(p) == 4 && !exists(tmp));
    CHECK(saveFileAtomic(p, "second\n", 7) && sizeOf(p) == 7 && !exists(tmp));  // over an existing file
    writeRaw(tmp, "stale", 5);                                                  // a stale temporary from a crash
    CHECK(saveFileAtomic(p, "third!!\n", 8) && sizeOf(p) == 8 && !exists(tmp));
    SaveFile s;
    CHECK(saveBegin(s, p));
    std::fwrite("abandoned", 1, 9, s.f);
    CHECK(!saveCommit(s, false) && sizeOf(p) == 8 && !exists(tmp));  // a failed write keeps the old file
    char nowhere[600];
    std::snprintf(nowhere, sizeof nowhere, "%s/no_such_dir/x.txt", g_dir);
    CHECK(!saveFileAtomic(nowhere, "x", 1));
    std::remove(p);
}

bool sameSettings(const Settings& a, const Settings& b) {
    return a.variant == b.variant && a.master == b.master && a.music == b.music && a.effects == b.effects && a.haptics == b.haptics &&
           a.musicHostOnly == b.musicHostOnly && a.swapSides == b.swapSides && a.autoAim == b.autoAim && a.autoNova == b.autoNova &&
           a.effectsTier == b.effectsTier && a.frameRate == b.frameRate && a.reduceFlashing == b.reduceFlashing && a.uiScale == b.uiScale &&
           a.noticeSeen == b.noticeSeen && a.probedTier == b.probedTier && a.hintsSeen == b.hintsSeen && a.runsEnded == b.runsEnded;
}

bool settingsInRange(const Settings& s) {
    return s.variant < 3 && s.master >= 0.0f && s.master <= 1.0f && s.music >= 0.0f && s.music <= 1.0f && s.effects >= 0.0f &&
           s.effects <= 1.0f && s.haptics < 3 && s.effectsTier < 5 && s.frameRate < 4 && s.uiScale < 4 && s.probedTier < 5 && s.hintsSeen < (1u << 11);
}

void testSettings() {
    std::printf("[save tests] settings.txt (the settings and the hints)\n");
    char p[512];
    pathOf(p, sizeof p, "settings.txt");
    std::remove(p);
    const Settings defaults;
    // Missing: nothing loaded, the defaults stand.
    Settings s;
    CHECK(!loadSettingsFile(p, s) && sameSettings(s, defaults));
    // A round trip, the hints among them.
    Settings w;
    w.variant = 2;
    w.master = 0.5f;
    w.music = 0.25f;
    w.haptics = 2;
    w.reduceFlashing = true;
    w.uiScale = 3;
    w.noticeSeen = true;
    w.hintsSeen = 0x5A5u;
    w.runsEnded = 7;
    CHECK(saveSettingsFile(p, w));
    Settings r;
    CHECK(loadSettingsFile(p, r) && sameSettings(r, w));
    // Short (cut after the first few lines): what is there loads, the rest stays default.
    const long full = sizeOf(p);
    truncateTo(p, 30);
    Settings t;
    CHECK(loadSettingsFile(p, t) && t.variant == 2 && t.hintsSeen == defaults.hintsSeen && t.uiScale == defaults.uiScale && settingsInRange(t));
    CHECK(full > 30);
    // Damaged values: every one held to its range (the hints to eleven bits).
    const char* bad = "variant=99\nmaster=nan\nmusic=-4\neffects=1e30\nhaptics=-1\neffects_tier=12\nframe_rate=40\nui_scale=9\nprobed_tier=77\n"
                      "hints_seen=4294967295\nruns_ended=x\nno equals sign here\n=\n\n";
    writeRaw(p, bad, std::strlen(bad));
    Settings d;
    CHECK(loadSettingsFile(p, d) && settingsInRange(d) && d.hintsSeen == 0x7FFu && d.master == 0.0f && d.music == 0.0f && d.effects == 1.0f);
    // Garbage: no crash, everything in range.
    for (unsigned k = 0; k < 8; ++k) {
        garbage(p, 1234u + k, 64u + 400u * k);
        Settings g;
        loadSettingsFile(p, g);
        CHECK(settingsInRange(g));
    }
    std::remove(p);
}

void testHighScores() {
    std::printf("[save tests] highscores.txt\n");
    char p[512];
    pathOf(p, sizeof p, "highscores.txt");
    std::remove(p);
    HighScores h;
    CHECK(!loadHighScoresFile(p, h) && h.count == 0);
    HighScores w;
    for (u32 k = 0; k < 12; ++k) {
        HighScore e;
        e.score = 1000u * (k + 1);
        e.track = 1 + k % 11;
        e.bestChain = 10 + k;
        e.seconds = 60 * k;
        e.kills = 100 * k;
        e.setComplete = k == 11;
        w.add(e);
    }
    CHECK(w.count == HighScores::kMax && w.best() == 12000u);
    CHECK(saveHighScoresFile(p, w));
    HighScores r;
    CHECK(loadHighScoresFile(p, r) && r.count == w.count && r.best() == 12000u && r.entry[0].setComplete && !r.entry[1].setComplete &&
          r.entry[0].track == w.entry[0].track && r.entry[9].score == w.entry[9].score);
    // Short: the whole lines before the cut load; a cut line is skipped or read short, never more than the table.
    truncateTo(p, sizeOf(p) / 2 + 3);
    HighScores t;
    CHECK(loadHighScoresFile(p, t) && t.count >= 4 && t.count <= HighScores::kMax && t.best() == 12000u);
    // Damaged lines are skipped.
    const char* bad = "run=abc\nrun=5 0 1 1 1\nrun=7 1000000 1 1 1 1\nrun=99999 3 4 5 6 1\nrun=\n=run\nrun=12 2\n";
    writeRaw(p, bad, std::strlen(bad));
    HighScores d;
    CHECK(loadHighScoresFile(p, d) && d.count == 1 && d.best() == 99999u && d.entry[0].track == 3 && d.entry[0].setComplete);
    for (unsigned k = 0; k < 8; ++k) {
        garbage(p, 99u + k, 64u + 500u * k);
        HighScores g;
        loadHighScoresFile(p, g);
        CHECK(g.count <= HighScores::kMax);
    }
    std::remove(p);
}

void testTierCache() {
    std::printf("[save tests] the tier cache\n");
    char p[512];
    pathOf(p, sizeof p, "tier.txt");
    std::remove(p);
    Tier t = Tier::Low;
    CHECK(!readTierCache(p, "Adreno (TM) 620", "512.1", t));
    const float ms[4] = {4.0f, 6.0f, 9.0f, 14.0f};
    CHECK(writeTierCache(p, "Adreno (TM) 620", "512.1", Tier::High, ms, 16.6f));
    Tier r = Tier::Low;
    CHECK(readTierCache(p, "Adreno (TM) 620", "512.1", r) && r == Tier::High);
    CHECK(!readTierCache(p, "another GPU", "512.1", r));  // a new GPU is probed again
    truncateTo(p, 40);
    CHECK(!readTierCache(p, "Adreno (TM) 620", "512.1", r));
    const char* bad = "raveform-tier 1\ngpu=Adreno (TM) 620\ndriver=512.1\ntier=enormous\n";
    writeRaw(p, bad, std::strlen(bad));
    CHECK(!readTierCache(p, "Adreno (TM) 620", "512.1", r));
    for (unsigned k = 0; k < 8; ++k) {
        garbage(p, 7u + k, 32u + 300u * k);
        CHECK(!readTierCache(p, "Adreno (TM) 620", "512.1", r));
    }
    std::remove(p);
}

void testBakedAudio() {
    std::printf("[save tests] the baked audio (one bake, about a second)\n");
    auto* music = new MusicData;
    auto* bank = new SampleBank;
    if (!music->load() || !bank->bake(*music, 48000)) {
        CHECK(!"music data or baking");
        delete bank;
        delete music;
        return;
    }
    char p[512];
    pathOf(p, sizeof p, "baked_audio.bin");
    std::remove(p);
    const u64 key = 0x1234ABCDull;
    auto* other = new SampleBank;
    CHECK(!other->loadCache(p, key) && other->count() == 0);  // missing
    CHECK(bank->saveCache(p, key));
    CHECK(other->loadCache(p, key) && other->count() == bank->count() && other->bytes() == bank->bytes());
    CHECK(!other->loadCache(p, key + 1) && other->count() == 0);  // another key: baked again
    const long full = sizeOf(p);
    // Damaged between a right header and tail: the last table (the recipes' counts) says more samples than there are.
    {
        FILE* f = std::fopen(p, "r+b");
        if (f) {
            std::fseek(f, full - 8, SEEK_SET);
            const u32 big = 0xFFFFFFFFu;
            std::fwrite(&big, sizeof big, 1, f);
            std::fclose(f);
        }
        CHECK(!other->loadCache(p, key) && other->count() == 0 && other->clap() == -1 && other->sfx(Sfx::Hit) == -1);
    }
    CHECK(bank->saveCache(p, key));
    truncateTo(p, 65536 < full / 2 ? 65536 : full / 2);  // short
    CHECK(!other->loadCache(p, key) && other->count() == 0);
    for (unsigned k = 0; k < 4; ++k) {
        garbage(p, 31u + k, 16u + 1000u * k);
        CHECK(!other->loadCache(p, key) && other->count() == 0);
    }
    // After all that the bank bakes and loads as new.
    CHECK(bank->saveCache(p, key) && other->loadCache(p, key) && other->count() == bank->count());
    std::remove(p);
    delete other;
    delete bank;
    delete music;
}
}  // namespace

int main() {
    const char* t = std::getenv("TEMP");
    if (!t || !*t) t = std::getenv("TMPDIR");
    std::snprintf(g_dir, sizeof g_dir, "%s", t && *t ? t : ".");
    testSaveFile();
    testSettings();
    testHighScores();
    testTierCache();
    testBakedAudio();
    std::printf("[save tests] %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

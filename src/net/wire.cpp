// The datagram layer under ENet (src/net/wire.h). ENet's protocol sources call the rf_enet_* functions below
// in place of its own socket and clock functions (src/net/enet_remap.h).

#include <enet/enet.h>  // first: on Windows it brings winsock2.h before anything includes windows.h

#include "net/wire.h"

#include "game/rng.h"

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rf::net {

namespace {

struct Spin {
    std::atomic_flag f;
    void lock() {
        while (f.test_and_set(std::memory_order_acquire)) {
        }
    }
    void unlock() { f.clear(std::memory_order_release); }
};
struct Hold {
    Spin& s;
    explicit Hold(Spin& sp) : s(sp) { s.lock(); }
    ~Hold() { s.unlock(); }
};

struct Datagram {
    i64 due = 0;     // held: when it leaves
    u32 seq = 0;     // held: order among equal due times
    u32 host = 0;    // held: destination; inbox: source
    u16 port = 0;
    u16 size = 0;
    u8 data[kMaxDatagram];
};

constexpr u32 kSockets = 16;
constexpr u32 kHeld = 256;   // sends the simulator holds back
constexpr u32 kInbox = 256;  // loopback: arrived, not yet read
constexpr u64 kHandleBase = 0x7A00;

struct VSocket {
    bool used = false;
    WireKind kind = WireKind::Udp;
    ENetSocket os = ENET_SOCKET_NULL;
    u32 host = 0;  // loopback: bound address (network order) and port
    u16 port = 0;
    NetSim sim;
    Rng rng{1};  // PCG32: independent draws (a weak generator made bursts of loss no real network makes)
    u32 seq = 0;
    Datagram* held = nullptr;
    u8* heldUsed = nullptr;
    u32 heldCount = 0;
    Datagram* inbox = nullptr;
    u32 inboxHead = 0, inboxCount = 0;
    Spin inboxLock;
    WireStats stats;
    // Bytes sent to each address (with IP/UDP headers): a host's downlink to each client, measured on the wire.
    struct Dest {
        u32 host = 0;
        u16 port = 0;
        u64 bytes = 0;
    } dest[8];
    u32 destCount = 0, destNext = 0;
};

VSocket g_sockets[kSockets];
Spin g_table;
std::atomic<u32> g_ephemeral{50000};
thread_local WireKind t_nextKind = WireKind::Udp;
thread_local NetSim t_nextSim;

std::atomic<bool> g_simClock{false};
std::atomic<i64> g_simUs{0};

VSocket* sock(ENetSocket h) {
    const u64 v = static_cast<u64>(h);
    if (v < kHandleBase || v >= kHandleBase + kSockets) return nullptr;
    VSocket* s = &g_sockets[v - kHandleBase];
    return s->used ? s : nullptr;
}
ENetSocket handleOf(u32 index) { return static_cast<ENetSocket>(kHandleBase + index); }

float rand01(Rng& r) { return r.uniform(); }

// Loopback: the socket bound to this port (127.0.0.1 or any address), or null.
VSocket* loopbackAt(u16 port) {
    for (VSocket& s : g_sockets)
        if (s.used && s.kind == WireKind::Loopback && s.port == port) return &s;
    return nullptr;
}

void deliverLoopback(VSocket& from, u32 dstHost, u16 dstPort, const u8* data, u16 size) {
    (void)dstHost;
    VSocket* to = nullptr;
    {
        Hold h(g_table);
        to = loopbackAt(dstPort);
    }
    if (!to) {
        ++from.stats.noRoute;
        return;
    }
    Hold h(to->inboxLock);
    if (to->inboxCount >= kInbox) {
        ++to->stats.queueFull;
        return;
    }
    Datagram& d = to->inbox[(to->inboxHead + to->inboxCount) % kInbox];
    d.host = from.host;
    d.port = from.port;
    d.size = size;
    std::memcpy(d.data, data, size);
    ++to->inboxCount;
}

void deliver(VSocket& s, u32 dstHost, u16 dstPort, const u8* data, u16 size) {
    if (s.kind == WireKind::Loopback) {
        deliverLoopback(s, dstHost, dstPort, data, size);
        return;
    }
    ENetAddress a;
    a.host = dstHost;
    a.port = dstPort;
    ENetBuffer b;
    b.data = const_cast<u8*>(data);
    b.dataLength = size;
    if (enet_socket_send(s.os, &a, &b, 1) < 0) ++s.stats.sendErrors;
}

bool ensureBuffers(VSocket& s) {
    if (!s.held) s.held = static_cast<Datagram*>(std::malloc(sizeof(Datagram) * kHeld));
    if (!s.heldUsed) s.heldUsed = static_cast<u8*>(std::calloc(kHeld, 1));
    if (!s.inbox) s.inbox = static_cast<Datagram*>(std::malloc(sizeof(Datagram) * kInbox));
    return s.held && s.heldUsed && s.inbox;
}

// ---- ENet's memory: fixed pools ------------------------------------------------------------------------

constexpr u32 kClasses = 6;
constexpr u32 kClassSize[kClasses] = {64, 128, 256, 512, 2048, 4096};
constexpr u32 kClassCount[kClasses] = {4096, 4096, 1024, 512, 1024, 64};
struct Pool {
    u8* base = nullptr;
    u32* freeList = nullptr;  // block indices
    u32 freeCount = 0;
} g_pools[kClasses];
Spin g_poolLock;
EnetPoolStats g_poolStats;
std::atomic<u32> g_initCount{0};

void* ENET_CALLBACK poolMalloc(size_t size) {
    {
        Hold h(g_poolLock);
        ++g_poolStats.allocs;
        for (u32 c = 0; c < kClasses; ++c) {
            if (size > kClassSize[c]) continue;
            Pool& p = g_pools[c];
            if (!p.freeCount) continue;  // a larger class may still have room
            const u32 b = p.freeList[--p.freeCount];
            ++g_poolStats.inUse[c];
            if (g_poolStats.inUse[c] > g_poolStats.peak[c]) g_poolStats.peak[c] = g_poolStats.inUse[c];
            return p.base + static_cast<size_t>(b) * kClassSize[c];
        }
        ++g_poolStats.fallbacks;
    }
    return std::malloc(size);
}

void ENET_CALLBACK poolFree(void* mem) {
    if (!mem) return;
    u8* m = static_cast<u8*>(mem);
    {
        Hold h(g_poolLock);
        ++g_poolStats.frees;
        for (u32 c = 0; c < kClasses; ++c) {
            Pool& p = g_pools[c];
            const size_t span = static_cast<size_t>(kClassCount[c]) * kClassSize[c];
            if (!p.base || m < p.base || m >= p.base + span) continue;
            p.freeList[p.freeCount++] = static_cast<u32>((m - p.base) / kClassSize[c]);
            --g_poolStats.inUse[c];
            return;
        }
    }
    std::free(mem);
}

void ENET_CALLBACK poolNoMemory() {
    Hold h(g_poolLock);
    ++g_poolStats.noMemory;
}

}  // namespace

// ---------------------------------------------------------------------------------------------- public

bool parseNetSim(const char* spec, NetSim& out) {
    if (!spec || !*spec) return true;
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s", spec);
    for (char* tok = std::strtok(buf, ","); tok; tok = std::strtok(nullptr, ",")) {
        char* eq = std::strchr(tok, '=');
        if (!eq) return false;
        *eq = 0;
        const float v = static_cast<float>(std::atof(eq + 1));
        if (v < 0.0f) return false;
        if (!std::strcmp(tok, "loss")) out.loss = v;
        else if (!std::strcmp(tok, "jitter")) out.jitterMs = v;
        else if (!std::strcmp(tok, "delay")) out.delayMs = v;
        else if (!std::strcmp(tok, "dup")) out.duplicate = v;
        else if (!std::strcmp(tok, "reorder")) out.reorder = v;
        else if (!std::strcmp(tok, "seed")) out.seed = static_cast<u32>(v);
        else return false;
    }
    return out.loss <= 1.0f && out.duplicate <= 1.0f && out.reorder <= 1.0f;
}

void wireSelect(WireKind kind, const NetSim& sim) {
    t_nextKind = kind;
    t_nextSim = sim;
}

void wirePump(u64 socketHandle) {
    VSocket* s = sock(static_cast<ENetSocket>(socketHandle));
    if (!s || !s->heldCount) return;
    const i64 now = wireNowUs();
    // Everything due, in order of due time (and of sending, for equal times).
    for (;;) {
        i32 best = -1;
        for (u32 k = 0; k < kHeld; ++k) {
            if (!s->heldUsed[k] || s->held[k].due > now) continue;
            if (best < 0 || s->held[k].due < s->held[best].due ||
                (s->held[k].due == s->held[best].due && static_cast<i32>(s->held[k].seq - s->held[best].seq) < 0))
                best = static_cast<i32>(k);
        }
        if (best < 0) return;
        const Datagram& d = s->held[best];
        deliver(*s, d.host, d.port, d.data, d.size);
        s->heldUsed[best] = 0;
        --s->heldCount;
    }
}

WireStats wireStats(u64 socketHandle) {
    const VSocket* s = sock(static_cast<ENetSocket>(socketHandle));
    return s ? s->stats : WireStats{};
}

void wireResetStats(u64 socketHandle) {
    VSocket* s = sock(static_cast<ENetSocket>(socketHandle));
    if (s) s->stats = WireStats{};
}

u64 wireSentTo(u64 socketHandle, u32 host, u16 port) {
    const VSocket* s = sock(static_cast<ENetSocket>(socketHandle));
    if (!s) return 0;
    for (u32 k = 0; k < s->destCount; ++k)
        if (s->dest[k].host == host && s->dest[k].port == port) return s->dest[k].bytes;
    return 0;
}

u32 loopbackHost() {
    ENetAddress a;
    enet_address_set_host_ip(&a, "127.0.0.1");
    return a.host;
}

void wireUseSimClock(bool on) { g_simClock.store(on); }
void wireSetSimTimeUs(i64 us) { g_simUs.store(us); }

i64 wireNowUs() {
    if (g_simClock.load(std::memory_order_relaxed)) return g_simUs.load(std::memory_order_relaxed);
    // Monotonic, offset so it never starts near zero (ENet treats a zero time as "never").
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() +
           1000000000ll;
}

bool netInit() {
    if (g_initCount.fetch_add(1) > 0) return true;
    for (u32 c = 0; c < kClasses; ++c) {
        Pool& p = g_pools[c];
        p.base = static_cast<u8*>(std::malloc(static_cast<size_t>(kClassCount[c]) * kClassSize[c]));
        p.freeList = static_cast<u32*>(std::malloc(sizeof(u32) * kClassCount[c]));
        if (!p.base || !p.freeList) return false;
        for (u32 b = 0; b < kClassCount[c]; ++b) p.freeList[b] = kClassCount[c] - 1 - b;
        p.freeCount = kClassCount[c];
        g_poolStats.blocks[c] = kClassCount[c];
        g_poolStats.blockSize[c] = kClassSize[c];
    }
    ENetCallbacks cb;
    std::memset(&cb, 0, sizeof cb);
    cb.malloc = poolMalloc;
    cb.free = poolFree;
    cb.no_memory = poolNoMemory;
    return enet_initialize_with_callbacks(ENET_VERSION, &cb) == 0;
}

void netShutdown() {
    if (g_initCount.load() == 0) return;
    if (g_initCount.fetch_sub(1) == 1) enet_deinitialize();  // the pools stay: ENet may still free into them
}

EnetPoolStats enetPoolStats() {
    Hold h(g_poolLock);
    return g_poolStats;
}

}  // namespace rf::net

// ---------------------------------------------------------------------------------------------- ENet's side

using namespace rf;
using namespace rf::net;

extern "C" {

ENetSocket rf_enet_socket_create(ENetSocketType type) {
    Hold h(g_table);
    for (u32 i = 0; i < kSockets; ++i) {
        VSocket& s = g_sockets[i];
        if (s.used) continue;
        if (!ensureBuffers(s)) return ENET_SOCKET_NULL;
        s.kind = t_nextKind;
        s.sim = t_nextSim;
        s.rng.reseed((static_cast<u64>(s.sim.seed) << 8) + i + 1);
        s.seq = 0;
        s.host = 0;
        s.port = 0;
        s.heldCount = 0;
        std::memset(s.heldUsed, 0, kHeld);
        s.inboxHead = s.inboxCount = 0;
        s.stats = WireStats{};
        s.destCount = s.destNext = 0;
        s.os = ENET_SOCKET_NULL;
        if (s.kind == WireKind::Udp) {
            s.os = enet_socket_create(type);
            if (s.os == ENET_SOCKET_NULL) return ENET_SOCKET_NULL;
        }
        s.used = true;
        return handleOf(i);
    }
    return ENET_SOCKET_NULL;
}

int rf_enet_socket_bind(ENetSocket h, const ENetAddress* address) {
    VSocket* s = sock(h);
    if (!s) return -1;
    if (s->kind == WireKind::Udp) return enet_socket_bind(s->os, address);
    Hold hold(g_table);
    u16 port = address ? address->port : 0;
    if (port && loopbackAt(port) && loopbackAt(port) != s) return -1;  // taken
    if (!port) port = static_cast<u16>(g_ephemeral.fetch_add(1));
    s->port = port;
    s->host = address && address->host ? address->host : loopbackHost();
    return 0;
}

int rf_enet_socket_get_address(ENetSocket h, ENetAddress* address) {
    VSocket* s = sock(h);
    if (!s || !address) return -1;
    if (s->kind == WireKind::Udp) return enet_socket_get_address(s->os, address);
    address->host = s->host;
    address->port = s->port;
    return 0;
}

int rf_enet_socket_set_option(ENetSocket h, ENetSocketOption option, int value) {
    VSocket* s = sock(h);
    if (!s) return -1;
    if (s->kind == WireKind::Udp) return enet_socket_set_option(s->os, option, value);
    return 0;
}

void rf_enet_socket_destroy(ENetSocket h) {
    VSocket* s = sock(h);
    if (!s) return;
    Hold hold(g_table);
    if (s->kind == WireKind::Udp && s->os != ENET_SOCKET_NULL) enet_socket_destroy(s->os);
    s->os = ENET_SOCKET_NULL;
    s->used = false;  // its buffers stay for the next socket
}

int rf_enet_socket_send(ENetSocket h, const ENetAddress* address, const ENetBuffer* buffers, size_t count) {
    VSocket* s = sock(h);
    if (!s || !address) return -1;
    u8 tmp[kMaxDatagram];
    size_t size = 0;
    for (size_t k = 0; k < count; ++k) size += buffers[k].dataLength;
    s->stats.sentBytes += size;
    ++s->stats.sentDatagrams;
    if (size > s->stats.maxDatagram) s->stats.maxDatagram = static_cast<u32>(size);
    {
        u32 d = 0;
        while (d < s->destCount && !(s->dest[d].host == address->host && s->dest[d].port == address->port)) ++d;
        if (d == s->destCount) {
            if (d < 8) {
                ++s->destCount;
            } else {
                d = s->destNext++ % 8;  // a new address in a full table takes an old one's place
            }
            s->dest[d] = VSocket::Dest{address->host, address->port, 0};
        }
        s->dest[d].bytes += size + kIpUdpHeaderBytes;
    }
    if (size > kMaxDatagram) {
        ++s->stats.oversize;  // never with ENet's MTU at 1392; the datagram is lost, ENet carries on
        return static_cast<int>(size);
    }
    size_t at = 0;
    for (size_t k = 0; k < count; ++k) {
        std::memcpy(tmp + at, buffers[k].data, buffers[k].dataLength);
        at += buffers[k].dataLength;
    }
    if (s->kind == WireKind::Loopback && s->port == 0) {
        // A client that never bound: it gets its address now, as an OS would on the first send.
        Hold hold(g_table);
        s->port = static_cast<u16>(g_ephemeral.fetch_add(1));
        s->host = loopbackHost();
    }
    const u16 sz = static_cast<u16>(size);
    if (!s->sim.active()) {
        if (s->kind == WireKind::Udp) {
            const int r = enet_socket_send(s->os, address, buffers, count);
            if (r < 0) ++s->stats.sendErrors;
            return r < 0 ? r : static_cast<int>(size);
        }
        deliver(*s, address->host, address->port, tmp, sz);
        return static_cast<int>(size);
    }
    // The condition simulator.
    const NetSim& c = s->sim;
    if (c.loss > 0.0f && rand01(s->rng) < c.loss) {
        ++s->stats.lost;
        return static_cast<int>(size);
    }
    const u32 copies = c.duplicate > 0.0f && rand01(s->rng) < c.duplicate ? 2u : 1u;
    if (copies == 2) ++s->stats.duplicated;
    const i64 now = wireNowUs();
    for (u32 k = 0; k < copies; ++k) {
        float ms = c.delayMs + c.jitterMs * rand01(s->rng);
        if (c.reorder > 0.0f && rand01(s->rng) < c.reorder) {
            ms += 10.0f + 2.0f * c.jitterMs * rand01(s->rng);
            ++s->stats.reordered;
        }
        u32 slot = kHeld;
        for (u32 j = 0; j < kHeld; ++j)
            if (!s->heldUsed[j]) {
                slot = j;
                break;
            }
        if (slot == kHeld) {
            ++s->stats.queueFull;
            deliver(*s, address->host, address->port, tmp, sz);  // nowhere to hold it: it goes now
            continue;
        }
        Datagram& d = s->held[slot];
        d.due = now + static_cast<i64>(ms * 1000.0f);
        d.seq = s->seq++;
        d.host = address->host;
        d.port = address->port;
        d.size = sz;
        std::memcpy(d.data, tmp, size);
        s->heldUsed[slot] = 1;
        ++s->heldCount;
    }
    // A datagram due now goes at once (no simulated delay left).
    wirePump(static_cast<u64>(h));
    return static_cast<int>(size);
}

int rf_enet_socket_receive(ENetSocket h, ENetAddress* address, ENetBuffer* buffers, size_t count) {
    VSocket* s = sock(h);
    if (!s || count < 1) return -1;
    if (s->kind == WireKind::Udp) {
        const int r = enet_socket_receive(s->os, address, buffers, count);
        if (r > 0) {
            s->stats.recvBytes += static_cast<u64>(r);
            ++s->stats.recvDatagrams;
        }
        return r;
    }
    Hold hold(s->inboxLock);
    if (!s->inboxCount) return 0;
    const Datagram& d = s->inbox[s->inboxHead];
    s->inboxHead = (s->inboxHead + 1) % kInbox;
    --s->inboxCount;
    if (d.size > buffers[0].dataLength) return -2;  // too large for ENet's buffer: dropped (cannot happen)
    std::memcpy(buffers[0].data, d.data, d.size);
    if (address) {
        address->host = d.host;
        address->port = d.port;
    }
    s->stats.recvBytes += d.size;
    ++s->stats.recvDatagrams;
    return d.size;
}

int rf_enet_socket_wait(ENetSocket h, enet_uint32* condition, enet_uint32 timeout) {
    VSocket* s = sock(h);
    if (!s) return -1;
    if (s->kind == WireKind::Udp) return enet_socket_wait(s->os, condition, timeout);
    // Loopback: never blocks (RAVEFORM services ENet with a zero timeout).
    if (condition) *condition = s->inboxCount ? ENET_SOCKET_WAIT_RECEIVE : ENET_SOCKET_WAIT_NONE;
    return 0;
}

enet_uint32 rf_enet_time_get(void) { return static_cast<enet_uint32>(wireNowUs() / 1000); }

}  // extern "C"

// ---------------------------------------------------------------------------------------------- plain datagrams

namespace rf::net {

namespace {
int g_lastError = 0;
int socketError() {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}
}  // namespace

u64 wireDatagramOpen(WireKind kind, const NetSim& sim, u16 port, bool broadcast) {
    wireSelect(kind, sim);
    const ENetSocket h = rf_enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (h == ENET_SOCKET_NULL) {
        g_lastError = socketError();
        return 0;
    }
    rf_enet_socket_set_option(h, ENET_SOCKOPT_NONBLOCK, 1);
    if (broadcast) rf_enet_socket_set_option(h, ENET_SOCKOPT_BROADCAST, 1);
    if (port) rf_enet_socket_set_option(h, ENET_SOCKOPT_REUSEADDR, 1);  // every process on this device may listen
    ENetAddress a;
    a.host = ENET_HOST_ANY;
    a.port = port;
    if (rf_enet_socket_bind(h, &a) < 0) {
        g_lastError = socketError();
        rf_enet_socket_destroy(h);
        return 0;
    }
    return static_cast<u64>(h);
}

bool wireDatagramSend(u64 socket, u32 host, u16 port, const void* data, u32 size) {
    ENetAddress a;
    a.host = ENET_HOST_TO_NET_32(host);
    a.port = port;
    ENetBuffer b;
    b.data = const_cast<void*>(data);
    b.dataLength = size;
    const int r = rf_enet_socket_send(static_cast<ENetSocket>(socket), &a, &b, 1);
    if (r < 0) g_lastError = socketError();
    return r >= 0;
}

int wireDatagramReceive(u64 socket, u32& host, u16& port, void* data, u32 capacity) {
    ENetAddress a;
    a.host = 0;
    a.port = 0;
    ENetBuffer b;
    b.data = data;
    b.dataLength = capacity;
    const int r = rf_enet_socket_receive(static_cast<ENetSocket>(socket), &a, &b, 1);
    if (r < 0) {
        g_lastError = socketError();
        return r;
    }
    host = ENET_NET_TO_HOST_32(a.host);
    port = a.port;
    return r;
}

void wireDatagramClose(u64 socket) { rf_enet_socket_destroy(static_cast<ENetSocket>(socket)); }

int wireLastError() { return g_lastError; }

}  // namespace rf::net

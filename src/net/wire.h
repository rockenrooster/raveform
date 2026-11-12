#pragma once
// The datagram layer under ENet (M4; TECH_SPEC 8.3, 8.4). ENet's protocol code sends and receives through
// these virtual sockets (src/net/enet_remap.h), which are either real UDP sockets or sockets of an
// in-process loopback network that runs a host and its clients in one process. A network condition
// simulator (delay, jitter, loss, duplication, reordering) sits under both: it holds back or drops what a
// socket sends. Every datagram is counted here, so bandwidth and the largest datagram (no IP
// fragmentation) are measured on the wire itself.
//
// ENet's memory comes from fixed pools taken at start-up (netInit), so a steady-state frame allocates
// nothing; a request the pools cannot serve falls back to malloc and is counted.
//
// The clock is the monotonic clock, or a simulated one (the loopback soak runs ten minutes of four peers in
// one thread faster than real time): ENet's timers and the simulator's delays both read it.
//
// Thread safety: sockets may be created, serviced and destroyed on different threads (the phone's
// in-process bot clients run on a worker); each socket is serviced by one thread at a time.

#include "core/types.h"

namespace rf::net {

// Network conditions applied to every datagram a socket sends.
struct NetSim {
    float delayMs = 0.0f;    // one way, every datagram
    float jitterMs = 0.0f;   // plus 0..jitter, uniform (datagrams overtake each other)
    float loss = 0.0f;       // probability a datagram is lost
    float duplicate = 0.0f;  // probability it arrives twice
    float reorder = 0.0f;    // probability it is held back a further 10 ms + 0..2 x jitter
    u32 seed = 1;
    bool active() const { return delayMs > 0.0f || jitterMs > 0.0f || loss > 0.0f || duplicate > 0.0f || reorder > 0.0f; }
};
// "loss=0.05,jitter=80,delay=10,dup=0.01,reorder=0.02" (any subset; percentages as fractions). False on a
// malformed spec.
bool parseNetSim(const char* spec, NetSim& out);

enum class WireKind : u8 { Udp, Loopback };

struct WireStats {
    u64 sentBytes = 0, recvBytes = 0;          // UDP payloads
    u64 sentDatagrams = 0, recvDatagrams = 0;
    u32 maxDatagram = 0;                       // the largest payload sent
    u32 lost = 0, duplicated = 0, reordered = 0;  // the condition simulator's doing
    u32 queueFull = 0, oversize = 0, noRoute = 0, sendErrors = 0;
};
constexpr u32 kIpUdpHeaderBytes = 28;  // IPv4 + UDP: counted into the bandwidth figures
constexpr u32 kMaxDatagram = 1472;     // a 1500-byte Ethernet MTU without IP fragmentation

// The next socket ENet creates (inside enet_host_create, on this thread) is of this kind, with these
// conditions on what it sends.
void wireSelect(WireKind kind, const NetSim& sim);
// Delivers what the socket's simulator held back and is now due; call before servicing its ENet host.
void wirePump(u64 socket);
WireStats wireStats(u64 socket);
void wireResetStats(u64 socket);
// Bytes the socket sent to one address since it was created (UDP payloads with IP/UDP headers).
u64 wireSentTo(u64 socket, u32 host, u16 port);

// Plain datagram sockets on the same layer (M4 stage B: the lobby's beacon), so the loopback network and the
// condition simulator work for them too. Addresses in host byte order; port 0 binds any. Non-blocking: receive
// returns the bytes read, 0 when nothing waits, < 0 on an error (wireLastError(): errno or WSAGetLastError; EPERM
// on Android 17 when the local network is not allowed).
u64 wireDatagramOpen(WireKind kind, const NetSim& sim, u16 port, bool broadcast);
bool wireDatagramSend(u64 socket, u32 host, u16 port, const void* data, u32 size);
int wireDatagramReceive(u64 socket, u32& host, u16& port, void* data, u32 capacity);
void wireDatagramClose(u64 socket);
int wireLastError();
// The loopback network's address of 127.0.0.1 (network order, as ENetAddress.host).
u32 loopbackHost();

// The clock (microseconds; ENet reads it in milliseconds).
void wireUseSimClock(bool on);
void wireSetSimTimeUs(i64 us);
i64 wireNowUs();

// ENet itself (enet_initialize_with_callbacks with the pools below). Counted: call once per user.
bool netInit();
void netShutdown();
struct EnetPoolStats {
    u64 allocs = 0, frees = 0, fallbacks = 0, noMemory = 0;
    u32 inUse[6] = {}, peak[6] = {}, blocks[6] = {}, blockSize[6] = {};
};
EnetPoolStats enetPoolStats();

}  // namespace rf::net

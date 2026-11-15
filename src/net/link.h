#pragma once
// One ENet host (M4; TECH_SPEC 8.3): a listening host with up to three clients, or a client with its one
// peer, on UDP or on the in-process loopback network (net/wire.h). Channels: 0 reliable ordered, 1
// unreliable sequenced, 2 lobby and control. ENet is configured for a LAN game: its unreliable-packet
// throttle off (packets are dropped only by the network), packets at most 4 KB (a size field can never
// make it allocate more), datagrams within the 1392-byte MTU (no IP fragmentation).

#include "core/types.h"
#include "net/wire.h"

namespace rf::net {

constexpr u16 kGamePort = 47778;    // TECH_SPEC 8.3
constexpr u16 kBeaconPort = 47777;  // TECH_SPEC 8.3 (discovery: stage B)
enum Channel : u8 { kChReliable = 0, kChUnreliable = 1, kChControl = 2, kChannels = 3 };
constexpr u32 kMaxLinkPeers = 4;
constexpr u32 kMaxPacket = 4096;  // ENet's maximumPacketSize
// A peer that has sent nothing for longer than this (every live peer sends 30 datagrams a second) is taken as gone or cut
// off until it is heard again: resends back off and a client holds its poses and claims (docs/PROGRESS_m4_net.md).
constexpr u32 kLinkSilentMs = 250;

struct LinkPeerStats {
    bool connected = false;
    u32 rttMs = 0, rttVarMs = 0;
    float loss = 0.0f;  // ENet's mean loss of reliable packets
    u32 sent = 0, lostPackets = 0;
    u32 silentMs = 0;   // since the last datagram from it
    u32 inFlight = 0;   // reliable bytes sent and not yet acknowledged
};

class Link {
public:
    struct Event {
        enum Type : u8 { None, Connect, Disconnect, Receive } type = None;
        u32 peer = 0;
        u8 channel = 0;
        const u8* data = nullptr;
        u32 size = 0;
        u32 info = 0;  // Connect: the data the client connected with; Disconnect: its reason
    };

    Link() = default;
    Link(const Link&) = delete;
    Link& operator=(const Link&) = delete;
    ~Link() { close(); }

    // Host: listen on bindIp ("" = every address) and port.
    bool listen(const char* bindIp, u16 port, u32 maxPeers, WireKind kind, const NetSim& sim);
    // Client: connect to ip:port (peer 0 once connected); `hello` reaches the host with the connect event.
    // `bindIp`: the local address to send from ("" = the OS's choice).
    bool connect(const char* ip, u16 port, WireKind kind, const NetSim& sim, u32 hello, const char* bindIp = "");
    void close();
    // Tests: the link goes without a word to its peers, as when the process is killed (they time it out).
    void abandon();
    bool open() const { return host_ != nullptr; }

    // Pumps the wire and services ENet without waiting: the next event, or false when there is none. A
    // received packet's data stays valid until the next call.
    bool poll(Event& out);
    bool send(u32 peer, u8 channel, const void* data, u32 size, bool reliable);
    void flush();
    void disconnect(u32 peer, u32 reason);
    // How long a silent peer is waited for before it is dropped: ms > 0 that long (pause-on-leave: a peer whose app left the
    // screen), 0 ENet's own (5 to 30 s).
    void setPeerTimeout(u32 peer, u32 ms);
    // The peer is forgotten at once, without a word (ENet's reset: no event follows; the caller handles it as a disconnect).
    void drop(u32 peer);
    bool connected(u32 peer) const;
    LinkPeerStats peerStats(u32 peer) const;
    WireStats wire() const;
    // Bytes sent to one peer on the wire since the link opened (with IP/UDP headers).
    u64 sentTo(u32 peer) const;
    u64 socket() const;
    u32 serviceErrors() const { return errors_; }

private:
    u32 errors_ = 0;
    void* host_ = nullptr;     // ENetHost*
    void* holding_ = nullptr;  // ENetPacket* handed out by the last poll
    bool pumped_ = false;
    u32 heard_[kMaxLinkPeers] = {};  // ENet's time (ms) a packet last came from each peer (0: never)
};

}  // namespace rf::net

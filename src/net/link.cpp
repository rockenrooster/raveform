#include <enet/enet.h>  // first (winsock2.h)

#include "net/link.h"

#include <cstring>

namespace rf::net {

namespace {
ENetHost* H(void* h) { return static_cast<ENetHost*>(h); }

void configure(ENetHost* h) {
    h->maximumPacketSize = kMaxPacket;   // no allocation from a hostile size field beyond this
    h->maximumWaitingData = 256 * 1024;
    h->mtu = ENET_HOST_DEFAULT_MTU;      // 1392: datagrams stay under 1472, no IP fragmentation
}

void configurePeer(ENetPeer* p) {
    // A LAN game: unreliable packets are never thrown away by ENet's throttle (the network loses them or not).
    enet_peer_throttle_configure(p, ENET_PEER_PACKET_THROTTLE_INTERVAL, ENET_PEER_PACKET_THROTTLE_SCALE, 0);
    enet_peer_timeout(p, ENET_PEER_TIMEOUT_LIMIT, ENET_PEER_TIMEOUT_MINIMUM, ENET_PEER_TIMEOUT_MAXIMUM);
}

// ENet doubles a reliable command's resend timeout at every loss (TCP's backoff). On a game's LAN link that turns a
// few losses in a row into seconds of a stalled reliable stream (at 5% loss each way about one command in 10,000
// fails four times: 0.24 + 0.47 + 0.95 + 1.9 s). Here a resend waits one round-trip timeout (ENet's own first one,
// from the current estimate), never more, while the peer is heard from (it sends 30 times a second). A peer silent
// for longer than kLinkSilentMs is gone or cut off: ENet's backoff then applies, so a link to a dead peer does not resend
// everything unacknowledged every 50 ms until ENet's timeout (5 s at least) ends it (M4: a client whose host vanished
// sent 300-764 kbit/s in its last second, docs/PROGRESS_m4_net.md). Heard again, the cap applies at the next flush. "Heard" is our own count of what arrived (Link::poll): ENet's lastReceiveTime moves only
// with acknowledgements, so a peer that is sending plenty but has nothing of ours to acknowledge looked silent.
void capResendTimeouts(ENetHost* h, const u32* heard) {
    for (size_t k = 0; k < h->peerCount; ++k) {
        ENetPeer& p = h->peers[k];
        if (p.state != ENET_PEER_STATE_CONNECTED || enet_list_empty(&p.sentReliableCommands)) continue;
        const enet_uint32 silent = k < kMaxLinkPeers && heard[k] ? h->serviceTime - heard[k] : 0u;  // wrapping milliseconds
        const bool gone = silent > kLinkSilentMs && silent < 0x80000000u;
        enet_uint32 cap = p.roundTripTime + 4 * p.roundTripTimeVariance;
        if (cap < 50) cap = 50;
        enet_uint32 next = 0;
        bool have = false;
        for (ENetListIterator it = enet_list_begin(&p.sentReliableCommands); it != enet_list_end(&p.sentReliableCommands);
             it = enet_list_next(it)) {
            ENetOutgoingCommand* c = reinterpret_cast<ENetOutgoingCommand*>(it);
            // Gone: ENet's own backoff (each resend waits twice as long), which also brings its timeout to an end (a
            // resend timeout held at the silence's length moved the deadline on forever: the link never ended).
            if (!gone && c->roundTripTimeout > cap) c->roundTripTimeout = cap;
            const enet_uint32 due = c->sentTime + c->roundTripTimeout;
            if (!have || static_cast<int>(due - next) < 0) {  // earlier (wrapping milliseconds)
                next = due;
                have = true;
            }
        }
        if (have) p.nextTimeout = next;
    }
}
}  // namespace

bool Link::listen(const char* bindIp, u16 port, u32 maxPeers, WireKind kind, const NetSim& sim) {
    close();
    ENetAddress a;
    a.host = ENET_HOST_ANY;
    a.port = port;
    if (bindIp && *bindIp && enet_address_set_host_ip(&a, bindIp) != 0) return false;
    wireSelect(kind, sim);
    ENetHost* h = enet_host_create(&a, maxPeers > kMaxLinkPeers ? kMaxLinkPeers : maxPeers, kChannels, 0, 0);
    if (!h) return false;
    configure(h);
    for (u32& v : heard_) v = 0;
    host_ = h;
    return true;
}

bool Link::connect(const char* ip, u16 port, WireKind kind, const NetSim& sim, u32 hello, const char* bindIp) {
    close();
    ENetAddress local;
    local.host = ENET_HOST_ANY;
    local.port = 0;
    const bool bind = bindIp && *bindIp;
    if (bind && enet_address_set_host_ip(&local, bindIp) != 0) return false;
    wireSelect(kind, sim);
    ENetHost* h = enet_host_create(bind ? &local : nullptr, 1, kChannels, 0, 0);
    if (!h) return false;
    configure(h);
    ENetAddress a;
    a.port = port;
    if (enet_address_set_host_ip(&a, ip) != 0) {
        enet_host_destroy(h);
        return false;
    }
    ENetPeer* p = enet_host_connect(h, &a, kChannels, hello);
    if (!p) {
        enet_host_destroy(h);
        return false;
    }
    // The throttle is configured once connected (poll): the handshake echoes the values it began with.
    for (u32& v : heard_) v = 0;
    host_ = h;
    return true;
}

void Link::close() {
    if (holding_) {
        enet_packet_destroy(static_cast<ENetPacket*>(holding_));
        holding_ = nullptr;
    }
    if (!host_) return;
    ENetHost* h = H(host_);
    for (size_t i = 0; i < h->peerCount; ++i)
        if (h->peers[i].state == ENET_PEER_STATE_CONNECTED) enet_peer_disconnect_now(&h->peers[i], 0);
    enet_host_flush(h);
    enet_host_destroy(h);
    host_ = nullptr;
}

void Link::abandon() {
    if (holding_) {
        enet_packet_destroy(static_cast<ENetPacket*>(holding_));
        holding_ = nullptr;
    }
    if (!host_) return;
    enet_host_destroy(H(host_));  // (resets its peers without sending anything)
    host_ = nullptr;
}

bool Link::poll(Event& out) {
    out = Event{};
    if (holding_) {
        enet_packet_destroy(static_cast<ENetPacket*>(holding_));
        holding_ = nullptr;
    }
    if (!host_) return false;
    ENetHost* h = H(host_);
    if (!pumped_) {
        wirePump(static_cast<u64>(h->socket));
        pumped_ = true;
    }
    ENetEvent ev;
    const int r = enet_host_service(h, &ev, 0);
    if (r < 0) ++errors_;
    if (r <= 0) {
        capResendTimeouts(h, heard_);
        pumped_ = false;  // the next poll (next frame) pumps again
        return false;
    }
    out.peer = static_cast<u32>(ev.peer - h->peers);
    if (out.peer < kMaxLinkPeers && ev.type != ENET_EVENT_TYPE_DISCONNECT) heard_[out.peer] = h->serviceTime ? h->serviceTime : 1u;
    switch (ev.type) {
        case ENET_EVENT_TYPE_CONNECT:
            configurePeer(ev.peer);
            out.type = Event::Connect;
            out.info = ev.data;
            return true;
        case ENET_EVENT_TYPE_DISCONNECT:
            out.type = Event::Disconnect;
            out.info = ev.data;
            return true;
        case ENET_EVENT_TYPE_RECEIVE:
            out.type = Event::Receive;
            out.channel = ev.channelID;
            out.data = ev.packet->data;
            out.size = static_cast<u32>(ev.packet->dataLength);
            holding_ = ev.packet;
            return true;
        default: return true;
    }
}

bool Link::send(u32 peer, u8 channel, const void* data, u32 size, bool reliable) {
    if (!host_ || size == 0 || size > kMaxPacket || channel >= kChannels) return false;
    ENetHost* h = H(host_);
    if (peer >= h->peerCount || h->peers[peer].state != ENET_PEER_STATE_CONNECTED) return false;
    // Unreliable packets go unsequenced: under jitter a sequenced channel throws away every packet that arrives
    // after a newer one (40% at 80 ms of jitter); corrections each carry other enemies, and poses and anchors
    // carry their own time, so the receiver orders them itself.
    ENetPacket* p = enet_packet_create(data, size, reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
    if (!p) return false;
    if (enet_peer_send(&h->peers[peer], channel, p) != 0) {
        enet_packet_destroy(p);
        return false;
    }
    return true;
}

void Link::flush() {
    if (!host_) return;
    enet_host_flush(H(host_));
    capResendTimeouts(H(host_), heard_);
}

void Link::disconnect(u32 peer, u32 reason) {
    if (!host_) return;
    ENetHost* h = H(host_);
    if (peer < h->peerCount) enet_peer_disconnect_later(&h->peers[peer], reason);
}

bool Link::connected(u32 peer) const {
    if (!host_) return false;
    ENetHost* h = H(host_);
    return peer < h->peerCount && h->peers[peer].state == ENET_PEER_STATE_CONNECTED;
}

LinkPeerStats Link::peerStats(u32 peer) const {
    LinkPeerStats s;
    if (!host_) return s;
    ENetHost* h = H(host_);
    if (peer >= h->peerCount) return s;
    const ENetPeer& p = h->peers[peer];
    s.connected = p.state == ENET_PEER_STATE_CONNECTED;
    s.rttMs = p.roundTripTime;
    s.rttVarMs = p.roundTripTimeVariance;
    s.loss = static_cast<float>(p.packetLoss) / static_cast<float>(ENET_PEER_PACKET_LOSS_SCALE);
    s.sent = p.packetsSent;
    s.lostPackets = p.packetsLost;
    const enet_uint32 silent = peer < kMaxLinkPeers ? h->serviceTime - heard_[peer] : 0u;  // wrapping milliseconds
    s.silentMs = s.connected && peer < kMaxLinkPeers && heard_[peer] && silent < 0x80000000u ? static_cast<u32>(silent) : 0u;
    s.inFlight = p.reliableDataInTransit;
    return s;
}

WireStats Link::wire() const { return host_ ? wireStats(static_cast<u64>(H(host_)->socket)) : WireStats{}; }
u64 Link::sentTo(u32 peer) const {
    if (!host_) return 0;
    ENetHost* h = H(host_);
    if (peer >= h->peerCount) return 0;
    return wireSentTo(static_cast<u64>(h->socket), h->peers[peer].address.host, h->peers[peer].address.port);
}
u64 Link::socket() const { return host_ ? static_cast<u64>(H(host_)->socket) : 0; }

}  // namespace rf::net

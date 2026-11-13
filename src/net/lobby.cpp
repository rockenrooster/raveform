// The protocol (and through it the world) before any system header: windows.h defines `far` and `near`.
#include "net/lobby.h"
#include "net/protocol.h"

#include <enet/enet.h>  // winsock2.h before iphlpapi.h

#include <cstring>

#if defined(_WIN32)
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace rf::net {

namespace {
constexpr u8 kMagic[4] = {'R', 'F', 'B', 'C'};

i32 symbolIndex(char c) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    for (i32 k = 0; k < 16; ++k)
        if (kRoomAlphabet[k] == c) return k;
    return -1;
}

void put16(u8* p, u16 v) {
    p[0] = static_cast<u8>(v);
    p[1] = static_cast<u8>(v >> 8);
}
void put32(u8* p, u32 v) {
    for (u32 k = 0; k < 4; ++k) p[k] = static_cast<u8>(v >> (8 * k));
}
u16 get16(const u8* p) { return static_cast<u16>(p[0] | (p[1] << 8)); }
u32 get32(const u8* p) { return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) | (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24); }

bool isPrivate(u32 a) {
    return (a >> 24) == 10 || (a >> 20) == ((172u << 4) | 1u) || (a >> 16) == ((192u << 8) | 168u);
}
}  // namespace

// ---------------------------------------------------------------------------------------------- room codes

void roomCodeEncode(u16 low16, char out[5]) {
    for (u32 k = 0; k < 4; ++k) out[k] = kRoomAlphabet[(low16 >> (12 - 4 * k)) & 15u];
    out[4] = '\0';
}

bool roomCodeDecode(const char* code, u16& low16) {
    if (!code) return false;
    u32 v = 0;
    for (u32 k = 0; k < 4; ++k) {
        const i32 i = symbolIndex(code[k]);
        if (i < 0) return false;
        v = (v << 4) | static_cast<u32>(i);
    }
    if (code[4] != '\0') return false;
    low16 = static_cast<u16>(v);
    return true;
}

// ---------------------------------------------------------------------------------------------- beacons

bool beaconWrite(const BeaconInfo& b, u8* out, u32 capacity) {
    if (!out || capacity < kBeaconBytes) return false;
    std::memcpy(out, kMagic, 4);
    put32(out + 4, b.version);
    put16(out + 8, b.port);
    put16(out + 10, b.code);
    out[12] = b.players;
    out[13] = b.seats;
    out[14] = b.state;
    out[15] = 0;  // reserved
    std::memset(out + 16, 0, 16);
    for (u32 k = 0; k < 15 && b.name[k]; ++k) out[16 + k] = static_cast<u8>(b.name[k]);
    return true;
}

bool beaconParse(const u8* data, u32 size, BeaconInfo& out) {
    if (!data || size != kBeaconBytes || std::memcmp(data, kMagic, 4) != 0) return false;
    BeaconInfo b;
    b.version = get32(data + 4);
    b.port = get16(data + 8);
    b.code = get16(data + 10);
    b.players = data[12];
    b.seats = data[13];
    b.state = data[14];
    if (data[15] != 0 || b.port == 0 || b.seats < 1 || b.seats > 4 || b.players > b.seats || b.state > kBeaconRunning) return false;
    bool ended = false;
    for (u32 k = 0; k < 16; ++k) {
        const u8 c = data[16 + k];
        if (c == 0) {
            ended = true;
            continue;
        }
        if (ended || c < 0x20 || c > 0x7E) return false;  // printable, then zeros only
        b.name[k] = static_cast<char>(c);
    }
    if (!ended) return false;
    b.compatible = b.version == kProtocolVersion;
    out = b;
    return true;
}

// ---------------------------------------------------------------------------------------------- local addresses

u32 localIPv4s(LocalIPv4* out, u32 capacity) {
    u32 n = 0;
    const auto add = [&](u32 a, u32 m) {
        if (!a || (a >> 24) == 127 || (a >> 16) == ((169u << 8) | 254u) || n >= capacity) return;  // loopback, link-local
        for (u32 k = 0; k < n; ++k)
            if (out[k].address == a) return;
        out[n++] = LocalIPv4{a, m};
    };
#if defined(_WIN32)
    ULONG size = 16 * 1024;
    alignas(8) static u8 buffer[64 * 1024];  // read rarely (the lobby), never in a run
    IP_ADAPTER_ADDRESSES* list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer);
    size = sizeof buffer;
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, list, &size) !=
        NO_ERROR)
        return 0;
    for (IP_ADAPTER_ADDRESSES* ad = list; ad; ad = ad->Next) {
        if (ad->OperStatus != IfOperStatusUp || ad->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (IP_ADAPTER_UNICAST_ADDRESS* ua = ad->FirstUnicastAddress; ua; ua = ua->Next) {
            const sockaddr* sa = ua->Address.lpSockaddr;
            if (!sa || sa->sa_family != AF_INET) continue;
            const u32 a = ntohl(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr);
            const u32 bits = ua->OnLinkPrefixLength <= 32 ? ua->OnLinkPrefixLength : 24;
            add(a, bits ? 0xFFFFFFFFu << (32 - bits) : 0u);
        }
    }
#else
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return 0;
    for (ifaddrs* it = list; it; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET || !(it->ifa_flags & IFF_UP) || (it->ifa_flags & IFF_LOOPBACK)) continue;
        const u32 a = ntohl(reinterpret_cast<const sockaddr_in*>(it->ifa_addr)->sin_addr.s_addr);
        const u32 m = it->ifa_netmask ? ntohl(reinterpret_cast<const sockaddr_in*>(it->ifa_netmask)->sin_addr.s_addr) : 0xFFFFFF00u;
        add(a, m);
    }
    freeifaddrs(list);
#endif
    // The private ranges first (192.168, then 10, then 172.16-31), in the order found.
    for (u32 i = 1; i < n; ++i) {
        const LocalIPv4 v = out[i];
        u32 j = i;
        const auto rank = [](u32 a) { return (a >> 16) == ((192u << 8) | 168u) ? 0 : ((a >> 24) == 10 ? 1 : (isPrivate(a) ? 2 : 3)); };
        while (j > 0 && rank(out[j - 1].address) > rank(v.address)) {
            out[j] = out[j - 1];
            --j;
        }
        out[j] = v;
    }
    return n;
}

u32 preferredLocalIPv4() {
    LocalIPv4 a[8];
    const u32 n = localIPv4s(a, 8);
    return n ? a[0].address : 0u;
}

// ---------------------------------------------------------------------------------------------- sender, browser

bool BeaconSender::start(WireKind kind, const NetSim& sim) {
    stop();
    kind_ = kind;
    socket_ = wireDatagramOpen(kind, sim, 0, true);
    nextUs_ = 0;
    sent_ = 0;
    return socket_ != 0;
}

void BeaconSender::stop() {
    if (socket_) wireDatagramClose(socket_);
    socket_ = 0;
}

void BeaconSender::update(i64 nowUs, const BeaconInfo& info) {
    if (!socket_ || nowUs < nextUs_) return;
    nextUs_ = nowUs + kBeaconEveryUs;
    u8 buf[kBeaconBytes];
    if (!beaconWrite(info, buf, sizeof buf)) return;
    // The limited broadcast, and each network's own (a PC with several adapters sends the first on one only).
    wireDatagramSend(socket_, 0xFFFFFFFFu, kBeaconUdpPort, buf, sizeof buf);
    if (kind_ == WireKind::Udp) {
        LocalIPv4 a[8];
        const u32 n = localIPv4s(a, 8);
        for (u32 k = 0; k < n; ++k)
            if (a[k].mask && a[k].mask != 0xFFFFFFFFu) wireDatagramSend(socket_, a[k].address | ~a[k].mask, kBeaconUdpPort, buf, sizeof buf);
    }
    ++sent_;
}

bool BeaconBrowser::start(WireKind kind) {
    stop();
    netRef_ = netInit();  // (Winsock on Windows: the join screen listens before any session has started the library)
    socket_ = wireDatagramOpen(kind, NetSim{}, kBeaconUdpPort, true);
    count_ = malformed_ = 0;
    lastError_ = socket_ ? 0 : wireLastError();
    return socket_ != 0;
}

void BeaconBrowser::stop() {
    if (socket_) wireDatagramClose(socket_);
    socket_ = 0;
    count_ = 0;
    if (netRef_) netShutdown();
    netRef_ = false;
}

void BeaconBrowser::poll(i64 nowUs) {
    if (!socket_) return;
    u8 buf[256];
    for (u32 guard = 0; guard < 64; ++guard) {
        u32 from = 0;
        u16 port = 0;
        const int n = wireDatagramReceive(socket_, from, port, buf, sizeof buf);
        if (n < 0) {
            lastError_ = wireLastError();
            break;
        }
        if (n == 0) break;
        BeaconInfo b;
        if (!beaconParse(buf, static_cast<u32>(n), b)) {
            ++malformed_;
            continue;
        }
        b.address = from;
        b.seenUs = nowUs;
        // One room, however many times it is heard (screens.md): a host on several networks sends its beacon to each,
        // and a joiner on more than one of them hears it from each of its addresses. A room is its code, game port and
        // name; it keeps the address it was first heard from, or the one its code was made from (the host's own).
        u32 at = count_;
        for (u32 k = 0; k < count_; ++k)
            if (rooms_[k].code == b.code && rooms_[k].port == b.port && std::strcmp(rooms_[k].name, b.name) == 0) at = k;
        if (at == count_) {
            if (count_ == kMaxRooms) continue;
            ++count_;
        } else if ((rooms_[at].address & 0xFFFFu) == b.code && (from & 0xFFFFu) != b.code) {
            b.address = rooms_[at].address;
        }
        rooms_[at] = b;
    }
    // Forget the hosts not heard for 3 s.
    u32 keep = 0;
    for (u32 k = 0; k < count_; ++k)
        if (nowUs - rooms_[k].seenUs <= kBeaconForgetUs) rooms_[keep++] = rooms_[k];
    count_ = keep;
}

const BeaconInfo* BeaconBrowser::byCode(u16 code) const {
    for (u32 k = 0; k < count_; ++k)
        if (rooms_[k].code == code) return &rooms_[k];
    return nullptr;
}

}  // namespace rf::net

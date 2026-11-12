#pragma once
// Discovery and room codes (M4 stage B; TECH_SPEC 8.3).
//
// A host broadcasts a beacon on UDP 47777 twice a second: magic, protocol version, the game port, the room code,
// the players seated and the seats, its state and its name. A browser listens on 47777 and keeps the hosts heard in
// the last 3 s. Beacons are parsed like every packet: the length exact, every field checked, nothing allocated.
//
// The room code is four characters for the low 16 bits of the host's IPv4 address, so it works within one /16
// (a home network, a phone's hotspot): a joiner puts its own address's high 16 bits in front. Sixteen symbols, so a
// character carries four bits; the ones easily confused (0 O, 1 I L, 2 Z, 5 S, 8 B, U V) are left out.

#include "core/types.h"
#include "net/wire.h"

namespace rf::net {

constexpr u16 kBeaconUdpPort = 47777;          // TECH_SPEC 8.3
constexpr i64 kBeaconEveryUs = 500000;         // twice a second
constexpr i64 kBeaconForgetUs = 3000000;       // a host not heard for 3 s leaves the list
constexpr u32 kBeaconBytes = 32;
constexpr const char* kRoomAlphabet = "34679ACDFHJKMPQR";

// Four characters (and a terminating zero) for the low 16 bits of an IPv4 address.
void roomCodeEncode(u16 low16, char out[5]);
// The 16 bits of a typed code (upper or lower case); false when it is not four of the alphabet's characters.
bool roomCodeDecode(const char* code, u16& low16);
// The host's address (host byte order) from a code and an address of this device on the same /16.
inline u32 roomCodeHost(u16 low16, u32 localIPv4) { return (localIPv4 & 0xFFFF0000u) | low16; }

enum BeaconState : u8 { kBeaconLobby = 0, kBeaconRunning = 1 };

struct BeaconInfo {
    u32 version = 0;
    u16 port = 0;          // the game port (47778)
    u16 code = 0;          // the room code's 16 bits
    u8 players = 0;        // seated
    u8 seats = 0;          // the room's seats (4)
    u8 state = kBeaconLobby;
    char name[16] = {};    // printable, zero-terminated
    // Filled in by the browser:
    u32 address = 0;       // where it came from (host byte order)
    i64 seenUs = 0;
    bool compatible = false;  // the same protocol version as this build
};

// The wire form; false (nothing written) when the buffer is short.
bool beaconWrite(const BeaconInfo& b, u8* out, u32 capacity);
// Validates a datagram as a beacon: exact length, magic, players <= seats <= 4, a known state, a printable name.
bool beaconParse(const u8* data, u32 size, BeaconInfo& out);

// This device's IPv4 addresses on its networks (not loopback), the private ranges first (host byte order).
struct LocalIPv4 {
    u32 address = 0, mask = 0;
};
u32 localIPv4s(LocalIPv4* out, u32 capacity);
// The address a room code is made from: the first private one (0 when none).
u32 preferredLocalIPv4();

// The host's beacon: one datagram to the broadcast address of every network this device is on, twice a second.
class BeaconSender {
public:
    ~BeaconSender() { stop(); }
    bool start(WireKind kind, const NetSim& sim);
    void stop();
    void update(i64 nowUs, const BeaconInfo& info);  // sends when one is due
    u32 sent() const { return sent_; }

private:
    u64 socket_ = 0;
    i64 nextUs_ = 0;
    u32 sent_ = 0;
    WireKind kind_ = WireKind::Udp;
};

// The joiner's list of rooms: the hosts heard in the last 3 s.
class BeaconBrowser {
public:
    static constexpr u32 kMaxRooms = 8;
    ~BeaconBrowser() { stop(); }
    bool start(WireKind kind);
    void stop();
    bool open() const { return socket_ != 0; }
    void poll(i64 nowUs);  // reads what arrived, forgets the silent
    u32 count() const { return count_; }
    const BeaconInfo& room(u32 i) const { return rooms_[i]; }
    // The room whose code is this, if one is heard (a typed code finds its host without the /16 rule).
    const BeaconInfo* byCode(u16 code) const;
    u32 malformed() const { return malformed_; }
    int lastError() const { return lastError_; }  // the socket's (EPERM: the local network is not allowed)

private:
    u64 socket_ = 0;
    BeaconInfo rooms_[kMaxRooms];
    u32 count_ = 0, malformed_ = 0;
    int lastError_ = 0;
};

}  // namespace rf::net

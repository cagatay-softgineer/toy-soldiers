#pragma once

#include "game/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace toy::net {

enum class MsgType : uint16_t {
	Hello = 1,       // C→H: name + version + reconnect token (0 = fresh)
	Welcome = 2,     // H→C: assigned seat + reconnect token
	LobbyState = 3,  // H→C: lobby/match snapshot
	MatchState = 4,  // H→C: full match snapshot
	Ready = 5,       // C→H: ready flag
	SetTower = 6,    // C→H: tower type
	StartMatch = 7,  // H internal / optional C request ignored
	PlayCard = 8,    // C→H: intentId, handIndex, target
	EndTurn = 9,     // C→H
	Reject = 10,     // H→C: reason string
	Chat = 11,       // either: text
	Goodbye = 12,
	SetCosmetics = 13, // C→H / host local: plastic, tower skin, accessory
	SetDeckMods = 14,  // v0.7 #47: ban 2 / add 2 sideboard defIds (lobby only)
	// --- v0.8 Reliable Party ---
	Heartbeat = 15,    // H→C every ~1s: u32 host millis (echo in Pong)
	Pong = 16,         // C→H: echoed millis (ping) — also freshens host-side timeout
	Kick = 17,         // H→C: reason string, then socket close (#79)
	ResyncRequest = 18, // C→H: send me a full snapshot (#92)
	RematchVote = 19,  // C→H: bool accept (#109)
	// --- v1.2 (protocol v7) ---
	CompressedState = 20, // H→C: u8 innerType + u32 rawSize + lz4 block (#95)
	DeltaState = 21,      // H→C: u32 baseSync + player-level delta (#96)
};

#pragma pack(push, 1)
struct MsgHeader {
	uint32_t magic; // 'TY2M'
	uint16_t type;
	uint16_t version;
};
#pragma pack(pop)

constexpr uint32_t kMsgMagic = 0x4D325954u; // 'TY2M' LE

std::vector<uint8_t> makeHello(const char* name, uint32_t reconnectToken = 0, bool spectate = false);
std::vector<uint8_t> makeWelcome(int seat, uint32_t reconnectToken);
std::vector<uint8_t> makeLobbyState(const std::vector<uint8_t>& snapshot);
std::vector<uint8_t> makeMatchState(const std::vector<uint8_t>& snapshot);
std::vector<uint8_t> makeReady(bool ready);
std::vector<uint8_t> makeSetTower(TowerType t);
std::vector<uint8_t> makeSetCosmetics(const Cosmetics& cos);
std::vector<uint8_t> makeSetDeckMods(const int banned[2], const int extras[2]);
std::vector<uint8_t> makePlayCard(uint32_t intentId, int handIndex, int targetPlayer);
std::vector<uint8_t> makeEndTurn();
std::vector<uint8_t> makeReject(const char* reason);
std::vector<uint8_t> makeChat(const char* text);
std::vector<uint8_t> makeHeartbeat(uint32_t millis);
std::vector<uint8_t> makePong(uint32_t echoedMillis);
std::vector<uint8_t> makeKick(const char* reason);
std::vector<uint8_t> makeResyncRequest();
std::vector<uint8_t> makeRematchVote(bool accept);
// v1.2 #95: wrap a snapshot-ish message payload in an lz4 block when it pays off.
std::vector<uint8_t> makeCompressedState(uint8_t innerType, const uint8_t* raw, size_t rawSize);
bool readCompressedState(const uint8_t* p, size_t n, uint8_t& innerType, std::vector<uint8_t>& rawOut);
// v1.2 #96: delta snapshot referencing the sync generation it was diffed against.
std::vector<uint8_t> makeDeltaState(const std::vector<uint8_t>& deltaPayload);

// Parses magic + header. Version is NOT rejected here — callers check
// hdr.version so mismatches can get a readable reject (#88).
bool parseHeader(const uint8_t* data, size_t size, MsgHeader& out, const uint8_t*& payload, size_t& payloadSize);

// Payload helpers
bool readHello(const uint8_t* p, size_t n, char nameOut[kPlayerNameLen], uint16_t& version, uint32_t& token,
			   bool& spectate);
bool readWelcome(const uint8_t* p, size_t n, int& seat, uint32_t& token);
bool readReady(const uint8_t* p, size_t n, bool& ready);
bool readSetTower(const uint8_t* p, size_t n, TowerType& t);
bool readSetCosmetics(const uint8_t* p, size_t n, Cosmetics& cos);
bool readSetDeckMods(const uint8_t* p, size_t n, int bannedOut[2], int extrasOut[2]);
bool readPlayCard(const uint8_t* p, size_t n, uint32_t& intentId, int& handIndex, int& target);
bool readString(const uint8_t* p, size_t n, std::string& out);
bool readU32(const uint8_t* p, size_t n, uint32_t& out);
bool readRematchVote(const uint8_t* p, size_t n, bool& accept);

// Snapshot messages: payload is raw snapshot bytes after header.
bool snapshotPayload(const uint8_t* p, size_t n, const uint8_t*& snap, size_t& snapSize);

// --- v0.8 #85: LAN lobby beacon (UDP datagram, not framed) ---

constexpr uint32_t kBeaconMagic = 0x43425954u; // 'TYBC' LE

struct BeaconInfo {
	char code[kRoomCodeLen] = {};      // 4-letter room code
	uint16_t version = 0;              // protocol version
	uint16_t tcpPort = 0;              // host lobby port
	char hostName[kPlayerNameLen] = {};
	uint8_t seatsTaken = 0;
	uint8_t seatsTotal = 0;
	// Filled by the receiver:
	char fromIp[64] = {};
};

std::vector<uint8_t> packBeacon(const BeaconInfo& b);
bool parseBeacon(const uint8_t* data, size_t size, BeaconInfo& out);

} // namespace toy::net

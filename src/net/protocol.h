#pragma once

#include "game/types.h"

#include <cstdint>
#include <vector>

namespace toy::net {

enum class MsgType : uint16_t {
	Hello = 1,       // C→H: name + version
	Welcome = 2,     // H→C: assigned seat
	LobbyState = 3,  // H→C: lobby/match snapshot
	MatchState = 4,  // H→C: full match snapshot
	Ready = 5,       // C→H: ready flag
	SetTower = 6,    // C→H: tower type
	StartMatch = 7,  // H internal / optional C request ignored
	PlayCard = 8,    // C→H: handIndex, target
	EndTurn = 9,     // C→H
	Reject = 10,     // H→C: reason string
	Chat = 11,       // either: text
	Goodbye = 12,
	SetCosmetics = 13, // C→H / host local: plastic, tower skin, accessory
};

#pragma pack(push, 1)
struct MsgHeader {
	uint32_t magic; // 'TY2M'
	uint16_t type;
	uint16_t version;
};
#pragma pack(pop)

constexpr uint32_t kMsgMagic = 0x4D325954u; // 'TY2M' LE

std::vector<uint8_t> makeHello(const char* name);
std::vector<uint8_t> makeWelcome(int seat, uint16_t port);
std::vector<uint8_t> makeLobbyState(const std::vector<uint8_t>& snapshot);
std::vector<uint8_t> makeMatchState(const std::vector<uint8_t>& snapshot);
std::vector<uint8_t> makeReady(bool ready);
std::vector<uint8_t> makeSetTower(TowerType t);
std::vector<uint8_t> makeSetCosmetics(const Cosmetics& cos);
std::vector<uint8_t> makePlayCard(int handIndex, int targetPlayer);
std::vector<uint8_t> makeEndTurn();
std::vector<uint8_t> makeReject(const char* reason);
std::vector<uint8_t> makeChat(const char* text);

bool parseHeader(const uint8_t* data, size_t size, MsgHeader& out, const uint8_t*& payload, size_t& payloadSize);

// Payload helpers
bool readHello(const uint8_t* p, size_t n, char nameOut[kPlayerNameLen], uint16_t& version);
bool readWelcome(const uint8_t* p, size_t n, int& seat);
bool readReady(const uint8_t* p, size_t n, bool& ready);
bool readSetTower(const uint8_t* p, size_t n, TowerType& t);
bool readSetCosmetics(const uint8_t* p, size_t n, Cosmetics& cos);
bool readPlayCard(const uint8_t* p, size_t n, int& handIndex, int& target);
bool readString(const uint8_t* p, size_t n, std::string& out);

// Snapshot messages: payload is raw snapshot bytes after header.
bool snapshotPayload(const uint8_t* p, size_t n, const uint8_t*& snap, size_t& snapSize);

} // namespace toy::net

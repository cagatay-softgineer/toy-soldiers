#include "net/protocol.h"

#include "lz4.h" // v1.2 #95 (vendored, extern/lz4)

#include <cstdio>
#include <cstring>
#include <string>

namespace toy::net {

namespace {

void writeHeader(std::vector<uint8_t>& out, MsgType type)
{
	MsgHeader h{};
	h.magic = kMsgMagic;
	h.type = static_cast<uint16_t>(type);
	h.version = kProtocolVersion;
	const auto* b = reinterpret_cast<const uint8_t*>(&h);
	out.insert(out.end(), b, b + sizeof(h));
}

void writeI32(std::vector<uint8_t>& out, int32_t v)
{
	const uint32_t u = static_cast<uint32_t>(v);
	out.push_back(static_cast<uint8_t>(u & 0xff));
	out.push_back(static_cast<uint8_t>((u >> 8) & 0xff));
	out.push_back(static_cast<uint8_t>((u >> 16) & 0xff));
	out.push_back(static_cast<uint8_t>((u >> 24) & 0xff));
}

void writeU16(std::vector<uint8_t>& out, uint16_t v)
{
	out.push_back(static_cast<uint8_t>(v & 0xff));
	out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
}

void writeStr24(std::vector<uint8_t>& out, const char* s)
{
	char tmp[kPlayerNameLen] = {};
	if (s) {
		std::snprintf(tmp, sizeof(tmp), "%s", s);
	}
	out.insert(out.end(), tmp, tmp + kPlayerNameLen);
}

int32_t readI32(const uint8_t*& p, const uint8_t* end, bool& ok)
{
	if (end - p < 4) {
		ok = false;
		return 0;
	}
	const int32_t v = static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
	p += 4;
	return v;
}

uint16_t readU16(const uint8_t*& p, const uint8_t* end, bool& ok)
{
	if (end - p < 2) {
		ok = false;
		return 0;
	}
	const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
	p += 2;
	return v;
}

} // namespace

std::vector<uint8_t> makeHello(const char* name, uint32_t reconnectToken, bool spectate)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Hello);
	writeStr24(out, name);
	writeU16(out, kProtocolVersion);
	writeI32(out, static_cast<int32_t>(reconnectToken));
	out.push_back(spectate ? 1 : 0); // v1.2 #111
	return out;
}

std::vector<uint8_t> makeWelcome(int seat, uint32_t reconnectToken)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Welcome);
	writeI32(out, seat);
	writeI32(out, static_cast<int32_t>(reconnectToken));
	return out;
}

std::vector<uint8_t> makeLobbyState(const std::vector<uint8_t>& snapshot)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::LobbyState);
	out.insert(out.end(), snapshot.begin(), snapshot.end());
	return out;
}

std::vector<uint8_t> makeMatchState(const std::vector<uint8_t>& snapshot)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::MatchState);
	out.insert(out.end(), snapshot.begin(), snapshot.end());
	return out;
}

std::vector<uint8_t> makeReady(bool ready)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Ready);
	out.push_back(ready ? 1 : 0);
	return out;
}

std::vector<uint8_t> makeSetTower(TowerType t)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::SetTower);
	out.push_back(static_cast<uint8_t>(t));
	return out;
}

std::vector<uint8_t> makeSetCosmetics(const Cosmetics& cos)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::SetCosmetics);
	out.push_back(static_cast<uint8_t>(cos.plastic));
	out.push_back(static_cast<uint8_t>(cos.towerSkin));
	out.push_back(static_cast<uint8_t>(cos.accessory));
	return out;
}

std::vector<uint8_t> makePlayCard(uint32_t intentId, int handIndex, int targetPlayer)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::PlayCard);
	writeI32(out, static_cast<int32_t>(intentId));
	writeI32(out, handIndex);
	writeI32(out, targetPlayer);
	return out;
}

std::vector<uint8_t> makeEndTurn()
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::EndTurn);
	return out;
}

std::vector<uint8_t> makeReject(const char* reason)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Reject);
	const char* r = reason ? reason : "rejected";
	const uint16_t len = static_cast<uint16_t>(std::strlen(r) > 200 ? 200 : std::strlen(r));
	writeU16(out, len);
	out.insert(out.end(), r, r + len);
	return out;
}

std::vector<uint8_t> makeChat(const char* text)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Chat);
	const char* r = text ? text : "";
	const uint16_t len = static_cast<uint16_t>(std::strlen(r) > 200 ? 200 : std::strlen(r));
	writeU16(out, len);
	out.insert(out.end(), r, r + len);
	return out;
}

bool parseHeader(const uint8_t* data, size_t size, MsgHeader& out, const uint8_t*& payload, size_t& payloadSize)
{
	if (size < sizeof(MsgHeader)) {
		return false;
	}
	std::memcpy(&out, data, sizeof(MsgHeader));
	if (out.magic != kMsgMagic) {
		return false;
	}
	// v0.8 #88: version intentionally NOT checked here — callers reject with a message.
	payload = data + sizeof(MsgHeader);
	payloadSize = size - sizeof(MsgHeader);
	return true;
}

bool readHello(const uint8_t* p, size_t n, char nameOut[kPlayerNameLen], uint16_t& version, uint32_t& token,
			   bool& spectate)
{
	if (n < kPlayerNameLen + 2) {
		return false;
	}
	std::memcpy(nameOut, p, kPlayerNameLen);
	nameOut[kPlayerNameLen - 1] = '\0';
	version = static_cast<uint16_t>(p[kPlayerNameLen] | (p[kPlayerNameLen + 1] << 8));
	token = 0;
	spectate = false;
	if (n >= kPlayerNameLen + 6) {
		bool ok = true;
		const uint8_t* cur = p + kPlayerNameLen + 2;
		const uint8_t* end = p + n;
		token = static_cast<uint32_t>(readI32(cur, end, ok));
		if (!ok) {
			token = 0;
		}
	}
	if (n >= kPlayerNameLen + 7) {
		spectate = p[kPlayerNameLen + 6] != 0; // v1.2 #111
	}
	return true;
}

bool readWelcome(const uint8_t* p, size_t n, int& seat, uint32_t& token)
{
	bool ok = true;
	const uint8_t* cur = p;
	const uint8_t* end = p + n;
	seat = readI32(cur, end, ok);
	token = static_cast<uint32_t>(readI32(cur, end, ok));
	return ok;
}

bool readReady(const uint8_t* p, size_t n, bool& ready)
{
	if (n < 1) {
		return false;
	}
	ready = p[0] != 0;
	return true;
}

bool readSetTower(const uint8_t* p, size_t n, TowerType& t)
{
	if (n < 1) {
		return false;
	}
	if (p[0] >= static_cast<uint8_t>(TowerType::Count)) {
		return false; // v0.7: 4 tower types, reject anything else
	}
	t = static_cast<TowerType>(p[0]);
	return true;
}

std::vector<uint8_t> makeSetDeckMods(const int banned[2], const int extras[2])
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::SetDeckMods);
	writeI32(out, banned[0]);
	writeI32(out, banned[1]);
	writeI32(out, extras[0]);
	writeI32(out, extras[1]);
	return out;
}

bool readSetDeckMods(const uint8_t* p, size_t n, int bannedOut[2], int extrasOut[2])
{
	bool ok = true;
	const uint8_t* cur = p;
	const uint8_t* end = p + n;
	bannedOut[0] = readI32(cur, end, ok);
	bannedOut[1] = readI32(cur, end, ok);
	extrasOut[0] = readI32(cur, end, ok);
	extrasOut[1] = readI32(cur, end, ok);
	return ok;
}

bool readSetCosmetics(const uint8_t* p, size_t n, Cosmetics& cos)
{
	if (n < 3) {
		return false;
	}
	cos.plastic = static_cast<PlasticColor>(p[0]);
	cos.towerSkin = static_cast<TowerSkin>(p[1]);
	cos.accessory = static_cast<Accessory>(p[2]);
	if (static_cast<uint8_t>(cos.plastic) >= static_cast<uint8_t>(PlasticColor::Count)) {
		return false;
	}
	if (static_cast<uint8_t>(cos.towerSkin) >= static_cast<uint8_t>(TowerSkin::Count)) {
		return false;
	}
	if (static_cast<uint8_t>(cos.accessory) >= static_cast<uint8_t>(Accessory::Count)) {
		return false;
	}
	return true;
}

bool readPlayCard(const uint8_t* p, size_t n, uint32_t& intentId, int& handIndex, int& target)
{
	bool ok = true;
	const uint8_t* cur = p;
	const uint8_t* end = p + n;
	intentId = static_cast<uint32_t>(readI32(cur, end, ok));
	handIndex = readI32(cur, end, ok);
	target = readI32(cur, end, ok);
	return ok;
}

bool readU32(const uint8_t* p, size_t n, uint32_t& out)
{
	bool ok = true;
	const uint8_t* cur = p;
	const uint8_t* end = p + n;
	out = static_cast<uint32_t>(readI32(cur, end, ok));
	return ok;
}

bool readRematchVote(const uint8_t* p, size_t n, bool& accept)
{
	if (n < 1) {
		return false;
	}
	accept = p[0] != 0;
	return true;
}

std::vector<uint8_t> makeHeartbeat(uint32_t millis)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Heartbeat);
	writeI32(out, static_cast<int32_t>(millis));
	return out;
}

std::vector<uint8_t> makePong(uint32_t echoedMillis)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Pong);
	writeI32(out, static_cast<int32_t>(echoedMillis));
	return out;
}

std::vector<uint8_t> makeKick(const char* reason)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Kick);
	const char* r = reason ? reason : "kicked by host";
	const uint16_t len = static_cast<uint16_t>(std::strlen(r) > 200 ? 200 : std::strlen(r));
	writeU16(out, len);
	out.insert(out.end(), r, r + len);
	return out;
}

std::vector<uint8_t> makeResyncRequest()
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::ResyncRequest);
	return out;
}

std::vector<uint8_t> makeRematchVote(bool accept)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::RematchVote);
	out.push_back(accept ? 1 : 0);
	return out;
}

std::vector<uint8_t> makeCompressedState(uint8_t innerType, const uint8_t* raw, size_t rawSize)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::CompressedState);
	out.push_back(innerType);
	writeI32(out, static_cast<int32_t>(rawSize));
	const int bound = LZ4_compressBound(static_cast<int>(rawSize));
	std::vector<uint8_t> comp(static_cast<size_t>(bound));
	const int n = LZ4_compress_default(reinterpret_cast<const char*>(raw), reinterpret_cast<char*>(comp.data()),
									   static_cast<int>(rawSize), bound);
	if (n <= 0) {
		return {}; // caller falls back to the uncompressed message
	}
	out.insert(out.end(), comp.begin(), comp.begin() + n);
	return out;
}

bool readCompressedState(const uint8_t* p, size_t n, uint8_t& innerType, std::vector<uint8_t>& rawOut)
{
	if (n < 5) {
		return false;
	}
	innerType = p[0];
	bool ok = true;
	const uint8_t* cur = p + 1;
	const uint8_t* end = p + n;
	const int32_t rawSize = readI32(cur, end, ok);
	if (!ok || rawSize <= 0 || rawSize > (1 << 20)) {
		return false;
	}
	rawOut.resize(static_cast<size_t>(rawSize));
	const int got = LZ4_decompress_safe(reinterpret_cast<const char*>(cur), reinterpret_cast<char*>(rawOut.data()),
										static_cast<int>(end - cur), rawSize);
	return got == rawSize;
}

std::vector<uint8_t> makeDeltaState(const std::vector<uint8_t>& deltaPayload)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::DeltaState);
	out.insert(out.end(), deltaPayload.begin(), deltaPayload.end());
	return out;
}

std::vector<uint8_t> packBeacon(const BeaconInfo& b)
{
	std::vector<uint8_t> out;
	out.reserve(4 + kRoomCodeLen + 2 + 2 + kPlayerNameLen + 2);
	out.push_back(static_cast<uint8_t>(kBeaconMagic & 0xff));
	out.push_back(static_cast<uint8_t>((kBeaconMagic >> 8) & 0xff));
	out.push_back(static_cast<uint8_t>((kBeaconMagic >> 16) & 0xff));
	out.push_back(static_cast<uint8_t>((kBeaconMagic >> 24) & 0xff));
	out.insert(out.end(), b.code, b.code + kRoomCodeLen);
	writeU16(out, b.version);
	writeU16(out, b.tcpPort);
	out.insert(out.end(), b.hostName, b.hostName + kPlayerNameLen);
	out.push_back(b.seatsTaken);
	out.push_back(b.seatsTotal);
	return out;
}

bool parseBeacon(const uint8_t* data, size_t size, BeaconInfo& out)
{
	const size_t need = 4 + kRoomCodeLen + 2 + 2 + kPlayerNameLen + 2;
	if (!data || size < need) {
		return false;
	}
	const uint32_t magic =
		static_cast<uint32_t>(data[0] | (data[1] << 8) | (data[2] << 16) | (static_cast<uint32_t>(data[3]) << 24));
	if (magic != kBeaconMagic) {
		return false;
	}
	const uint8_t* p = data + 4;
	std::memcpy(out.code, p, kRoomCodeLen);
	out.code[kRoomCodeLen - 1] = '\0';
	p += kRoomCodeLen;
	out.version = static_cast<uint16_t>(p[0] | (p[1] << 8));
	p += 2;
	out.tcpPort = static_cast<uint16_t>(p[0] | (p[1] << 8));
	p += 2;
	std::memcpy(out.hostName, p, kPlayerNameLen);
	out.hostName[kPlayerNameLen - 1] = '\0';
	p += kPlayerNameLen;
	out.seatsTaken = p[0];
	out.seatsTotal = p[1];
	return true;
}

bool readString(const uint8_t* p, size_t n, std::string& out)
{
	bool ok = true;
	const uint8_t* cur = p;
	const uint8_t* end = p + n;
	const uint16_t len = readU16(cur, end, ok);
	if (!ok || static_cast<size_t>(end - cur) < len) {
		return false;
	}
	out.assign(reinterpret_cast<const char*>(cur), len);
	return true;
}

bool snapshotPayload(const uint8_t* p, size_t n, const uint8_t*& snap, size_t& snapSize)
{
	snap = p;
	snapSize = n;
	return n > 0;
}

} // namespace toy::net

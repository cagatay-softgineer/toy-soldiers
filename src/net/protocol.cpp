#include "net/protocol.h"

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

std::vector<uint8_t> makeHello(const char* name)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Hello);
	writeStr24(out, name);
	writeU16(out, kProtocolVersion);
	return out;
}

std::vector<uint8_t> makeWelcome(int seat, uint16_t /*port*/)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::Welcome);
	writeI32(out, seat);
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

std::vector<uint8_t> makePlayCard(int handIndex, int targetPlayer)
{
	std::vector<uint8_t> out;
	writeHeader(out, MsgType::PlayCard);
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
	if (out.version != kProtocolVersion) {
		return false;
	}
	payload = data + sizeof(MsgHeader);
	payloadSize = size - sizeof(MsgHeader);
	return true;
}

bool readHello(const uint8_t* p, size_t n, char nameOut[kPlayerNameLen], uint16_t& version)
{
	if (n < kPlayerNameLen + 2) {
		return false;
	}
	std::memcpy(nameOut, p, kPlayerNameLen);
	nameOut[kPlayerNameLen - 1] = '\0';
	version = static_cast<uint16_t>(p[kPlayerNameLen] | (p[kPlayerNameLen + 1] << 8));
	return true;
}

bool readWelcome(const uint8_t* p, size_t n, int& seat)
{
	bool ok = true;
	const uint8_t* cur = p;
	const uint8_t* end = p + n;
	seat = readI32(cur, end, ok);
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
	t = static_cast<TowerType>(p[0]);
	return true;
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

bool readPlayCard(const uint8_t* p, size_t n, int& handIndex, int& target)
{
	bool ok = true;
	const uint8_t* cur = p;
	const uint8_t* end = p + n;
	handIndex = readI32(cur, end, ok);
	target = readI32(cur, end, ok);
	return ok;
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

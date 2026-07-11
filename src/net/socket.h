#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace toy::net {

// Lightweight non-blocking TCP (Winsock on Windows, BSD sockets elsewhere).

bool netInit();
void netShutdown();

using SocketHandle = std::uintptr_t;
constexpr SocketHandle kInvalidSocket = static_cast<SocketHandle>(-1);

class TcpSocket {
public:
	TcpSocket() = default;
	~TcpSocket();
	TcpSocket(const TcpSocket&) = delete;
	TcpSocket& operator=(const TcpSocket&) = delete;
	TcpSocket(TcpSocket&& o) noexcept;
	TcpSocket& operator=(TcpSocket&& o) noexcept;

	bool connect(const char* host, uint16_t port);
	void close();
	bool valid() const { return sock_ != kInvalidSocket; }

	// Non-blocking. Returns bytes sent/received, 0 = would block / closed, -1 = error.
	int send(const void* data, int len);
	int recv(void* data, int capacity);
	bool setNonBlocking(bool enabled);

	SocketHandle raw() const { return sock_; }

	static TcpSocket adopt(SocketHandle s);

private:
	explicit TcpSocket(SocketHandle s)
		: sock_(s)
	{
	}
	SocketHandle sock_ = kInvalidSocket;
};

class TcpListener {
public:
	TcpListener() = default;
	~TcpListener();
	TcpListener(const TcpListener&) = delete;
	TcpListener& operator=(const TcpListener&) = delete;

	bool listen(uint16_t port, int backlog = 8);
	void close();
	bool valid() const { return sock_ != kInvalidSocket; }

	// Non-blocking accept. Returns invalid socket if none pending.
	TcpSocket accept();

	uint16_t port() const { return port_; }

private:
	SocketHandle sock_ = kInvalidSocket;
	uint16_t port_ = 0;
};

// v0.8 #85: non-blocking UDP for LAN lobby beacons.
class UdpSocket {
public:
	UdpSocket() = default;
	~UdpSocket();
	UdpSocket(const UdpSocket&) = delete;
	UdpSocket& operator=(const UdpSocket&) = delete;

	// Sender: unbound socket with SO_BROADCAST enabled.
	bool openSender();
	// Receiver: bind the given port (SO_REUSEADDR) for beacon listening.
	bool openReceiver(uint16_t port);
	void close();
	bool valid() const { return sock_ != kInvalidSocket; }

	// Datagram to host:port ("255.255.255.255" broadcasts). True if sent.
	bool sendTo(const char* host, uint16_t port, const void* data, int len);
	// Non-blocking. Returns bytes read (0 = none pending, -1 = error); fills sender IP.
	int recvFrom(void* data, int capacity, char ipOut[64], uint16_t& portOut);

private:
	SocketHandle sock_ = kInvalidSocket;
};

// Length-prefixed frames: [u32 size LE][payload]
// size is payload length only (max 1 MiB).
class FrameCodec {
public:
	static constexpr uint32_t kMaxFrame = 1u << 20;

	void reset();
	// Feed raw socket bytes. Completed frames appended to out.
	bool feed(const uint8_t* data, size_t len, std::vector<std::vector<uint8_t>>& out);
	static std::vector<uint8_t> pack(const uint8_t* payload, size_t len);
	static std::vector<uint8_t> pack(const std::vector<uint8_t>& payload);

private:
	std::vector<uint8_t> buf_;
};

// Best-effort send of entire buffer on non-blocking socket (queues remainder).
class SendQueue {
public:
	void clear();
	void enqueue(std::vector<uint8_t> frame);
	// Returns false on hard socket error.
	bool flush(TcpSocket& sock);
	bool empty() const { return q_.empty(); }

private:
	std::vector<std::vector<uint8_t>> q_;
	size_t headOff_ = 0;
};

std::string lastNetError();

} // namespace toy::net

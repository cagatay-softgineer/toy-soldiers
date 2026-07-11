#include "net/socket.h"

#include <cstdio>
#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SockLen = int;
#define TOY_CLOSESOCKET closesocket
#define TOY_IOCTL ioctlsocket
#define TOY_WOULDBLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using SockLen = socklen_t;
#define TOY_CLOSESOCKET ::close
#define TOY_IOCTL ioctl
#define TOY_WOULDBLOCK (errno == EWOULDBLOCK || errno == EAGAIN)
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif
#endif

namespace toy::net {

namespace {

int g_initCount = 0;

SocketHandle toHandle(decltype(INVALID_SOCKET) s)
{
	return static_cast<SocketHandle>(s);
}

auto fromHandle(SocketHandle h)
{
#if defined(_WIN32)
	return static_cast<SOCKET>(h);
#else
	return static_cast<int>(h);
#endif
}

} // namespace

bool netInit()
{
	if (g_initCount++ > 0) {
		return true;
	}
#if defined(_WIN32)
	WSADATA wsa{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		g_initCount = 0;
		return false;
	}
#endif
	return true;
}

void netShutdown()
{
	if (g_initCount <= 0) {
		return;
	}
	if (--g_initCount == 0) {
#if defined(_WIN32)
		WSACleanup();
#endif
	}
}

std::string lastNetError()
{
#if defined(_WIN32)
	return std::to_string(WSAGetLastError());
#else
	return std::strerror(errno);
#endif
}

TcpSocket::~TcpSocket()
{
	close();
}

TcpSocket::TcpSocket(TcpSocket&& o) noexcept
	: sock_(o.sock_)
{
	o.sock_ = kInvalidSocket;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& o) noexcept
{
	if (this != &o) {
		close();
		sock_ = o.sock_;
		o.sock_ = kInvalidSocket;
	}
	return *this;
}

TcpSocket TcpSocket::adopt(SocketHandle s)
{
	return TcpSocket(s);
}

void TcpSocket::close()
{
	if (sock_ != kInvalidSocket) {
		TOY_CLOSESOCKET(fromHandle(sock_));
		sock_ = kInvalidSocket;
	}
}

bool TcpSocket::setNonBlocking(bool enabled)
{
	if (!valid()) {
		return false;
	}
#if defined(_WIN32)
	u_long mode = enabled ? 1u : 0u;
	return TOY_IOCTL(fromHandle(sock_), FIONBIO, &mode) == 0;
#else
	int flags = fcntl(fromHandle(sock_), F_GETFL, 0);
	if (flags < 0) {
		return false;
	}
	if (enabled) {
		flags |= O_NONBLOCK;
	} else {
		flags &= ~O_NONBLOCK;
	}
	return fcntl(fromHandle(sock_), F_SETFL, flags) == 0;
#endif
}

bool TcpSocket::connect(const char* host, uint16_t port)
{
	close();
	if (!netInit()) {
		return false;
	}

	char portStr[16];
	std::snprintf(portStr, sizeof(portStr), "%u", static_cast<unsigned>(port));

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	addrinfo* result = nullptr;
	if (getaddrinfo(host, portStr, &hints, &result) != 0 || !result) {
		return false;
	}

	bool ok = false;
	for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		auto s = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (s == INVALID_SOCKET) {
			continue;
		}
		if (::connect(s, ptr->ai_addr, static_cast<SockLen>(ptr->ai_addrlen)) == SOCKET_ERROR) {
			TOY_CLOSESOCKET(s);
			continue;
		}
		sock_ = toHandle(s);
		setNonBlocking(true);
		ok = true;
		break;
	}
	freeaddrinfo(result);
	return ok;
}

int TcpSocket::send(const void* data, int len)
{
	if (!valid() || len <= 0) {
		return -1;
	}
#if defined(_WIN32)
	const int n = ::send(fromHandle(sock_), static_cast<const char*>(data), len, 0);
#else
	const int n = static_cast<int>(::send(fromHandle(sock_), data, static_cast<size_t>(len), 0));
#endif
	if (n == SOCKET_ERROR) {
		if (TOY_WOULDBLOCK) {
			return 0;
		}
		return -1;
	}
	return n;
}

int TcpSocket::recv(void* data, int capacity)
{
	if (!valid() || capacity <= 0) {
		return -1;
	}
#if defined(_WIN32)
	const int n = ::recv(fromHandle(sock_), static_cast<char*>(data), capacity, 0);
#else
	const int n = static_cast<int>(::recv(fromHandle(sock_), data, static_cast<size_t>(capacity), 0));
#endif
	if (n == 0) {
		return -1; // graceful close
	}
	if (n == SOCKET_ERROR) {
		if (TOY_WOULDBLOCK) {
			return 0;
		}
		return -1;
	}
	return n;
}

TcpListener::~TcpListener()
{
	close();
}

void TcpListener::close()
{
	if (sock_ != kInvalidSocket) {
		TOY_CLOSESOCKET(fromHandle(sock_));
		sock_ = kInvalidSocket;
	}
	port_ = 0;
}

bool TcpListener::listen(uint16_t port, int backlog)
{
	close();
	if (!netInit()) {
		return false;
	}

	auto s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		return false;
	}

	int yes = 1;
#if defined(_WIN32)
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		TOY_CLOSESOCKET(s);
		return false;
	}
	if (::listen(s, backlog) == SOCKET_ERROR) {
		TOY_CLOSESOCKET(s);
		return false;
	}

	sock_ = toHandle(s);
	port_ = port;

	// Non-blocking accept.
#if defined(_WIN32)
	u_long mode = 1;
	TOY_IOCTL(s, FIONBIO, &mode);
#else
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
	return true;
}

TcpSocket TcpListener::accept()
{
	if (!valid()) {
		return {};
	}
	sockaddr_in addr{};
	SockLen len = sizeof(addr);
	auto s = ::accept(fromHandle(sock_), reinterpret_cast<sockaddr*>(&addr), &len);
	if (s == INVALID_SOCKET) {
		return {};
	}
	TcpSocket out = TcpSocket::adopt(toHandle(s));
	out.setNonBlocking(true);
	return out;
}

UdpSocket::~UdpSocket()
{
	close();
}

void UdpSocket::close()
{
	if (sock_ != kInvalidSocket) {
		TOY_CLOSESOCKET(fromHandle(sock_));
		sock_ = kInvalidSocket;
	}
}

namespace {

bool makeNonBlockingUdp(SocketHandle h)
{
#if defined(_WIN32)
	u_long mode = 1;
	return TOY_IOCTL(fromHandle(h), FIONBIO, &mode) == 0;
#else
	const int fd = fromHandle(h);
	const int flags = fcntl(fd, F_GETFL, 0);
	return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

} // namespace

bool UdpSocket::openSender()
{
	close();
	if (!netInit()) {
		return false;
	}
	auto s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		return false;
	}
	int yes = 1;
#if defined(_WIN32)
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
#endif
	sock_ = toHandle(s);
	makeNonBlockingUdp(sock_);
	return true;
}

bool UdpSocket::openReceiver(uint16_t port)
{
	close();
	if (!netInit()) {
		return false;
	}
	auto s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		return false;
	}
	int yes = 1;
#if defined(_WIN32)
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		TOY_CLOSESOCKET(s);
		return false;
	}
	sock_ = toHandle(s);
	makeNonBlockingUdp(sock_);
	return true;
}

bool UdpSocket::sendTo(const char* host, uint16_t port, const void* data, int len)
{
	if (!valid() || !host || len <= 0) {
		return false;
	}
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
		return false;
	}
	const int n = ::sendto(fromHandle(sock_), static_cast<const char*>(data), len, 0,
						   reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	return n == len;
}

int UdpSocket::recvFrom(void* data, int capacity, char ipOut[64], uint16_t& portOut)
{
	ipOut[0] = '\0';
	portOut = 0;
	if (!valid() || capacity <= 0) {
		return -1;
	}
	sockaddr_in addr{};
	SockLen alen = sizeof(addr);
	const int n = ::recvfrom(fromHandle(sock_), static_cast<char*>(data), capacity, 0,
							 reinterpret_cast<sockaddr*>(&addr), &alen);
	if (n == SOCKET_ERROR) {
		if (TOY_WOULDBLOCK) {
			return 0;
		}
		return -1;
	}
	inet_ntop(AF_INET, &addr.sin_addr, ipOut, 64);
	portOut = ntohs(addr.sin_port);
	return n;
}

void FrameCodec::reset()
{
	buf_.clear();
}

bool FrameCodec::feed(const uint8_t* data, size_t len, std::vector<std::vector<uint8_t>>& out)
{
	buf_.insert(buf_.end(), data, data + len);
	while (buf_.size() >= 4) {
		const uint32_t size = static_cast<uint32_t>(buf_[0] | (buf_[1] << 8) | (buf_[2] << 16) | (buf_[3] << 24));
		if (size > kMaxFrame) {
			return false;
		}
		if (buf_.size() < 4 + size) {
			break;
		}
		std::vector<uint8_t> frame(buf_.begin() + 4, buf_.begin() + 4 + static_cast<std::ptrdiff_t>(size));
		out.push_back(std::move(frame));
		buf_.erase(buf_.begin(), buf_.begin() + 4 + static_cast<std::ptrdiff_t>(size));
	}
	return true;
}

std::vector<uint8_t> FrameCodec::pack(const uint8_t* payload, size_t len)
{
	std::vector<uint8_t> out(4 + len);
	const uint32_t size = static_cast<uint32_t>(len);
	out[0] = static_cast<uint8_t>(size & 0xff);
	out[1] = static_cast<uint8_t>((size >> 8) & 0xff);
	out[2] = static_cast<uint8_t>((size >> 16) & 0xff);
	out[3] = static_cast<uint8_t>((size >> 24) & 0xff);
	if (len) {
		std::memcpy(out.data() + 4, payload, len);
	}
	return out;
}

std::vector<uint8_t> FrameCodec::pack(const std::vector<uint8_t>& payload)
{
	return pack(payload.data(), payload.size());
}

void SendQueue::clear()
{
	q_.clear();
	headOff_ = 0;
}

void SendQueue::enqueue(std::vector<uint8_t> frame)
{
	q_.push_back(std::move(frame));
}

bool SendQueue::flush(TcpSocket& sock)
{
	while (!q_.empty()) {
		auto& front = q_.front();
		const int remain = static_cast<int>(front.size() - headOff_);
		if (remain <= 0) {
			q_.erase(q_.begin());
			headOff_ = 0;
			continue;
		}
		const int n = sock.send(front.data() + headOff_, remain);
		if (n < 0) {
			return false;
		}
		if (n == 0) {
			return true; // would block
		}
		headOff_ += static_cast<size_t>(n);
		if (headOff_ >= front.size()) {
			q_.erase(q_.begin());
			headOff_ = 0;
		}
	}
	return true;
}

} // namespace toy::net

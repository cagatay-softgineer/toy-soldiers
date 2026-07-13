#pragma once

#include "net/socket.h"

namespace toy::net {

// v1.2 #89: connection-establishment seam. After a TCP stream is connected,
// FrameCodec/SendQueue/protocol treat it identically no matter how the
// connection came to exist — so the only thing that needs to be pluggable is
// *how* `sock` gets connected, not every send/recv call on the hot path.
//
// DirectTcp (the only implementation today) dials the peer's advertised
// IP:port straight over LAN/WAN — this is what every shipped build uses.
// Relay is the seam for a future backend (Steam Networking Sockets, or a
// small self-hosted relay/rendezvous service) that would hand back an
// already-connected stream instead of dialing the peer directly. There is no
// relay backend to connect to yet, so selecting it fails loudly rather than
// pretending to work — see docs/WAN_PLAY.md for what a real implementation
// would need to fill in.
enum class TransportKind : uint8_t {
	DirectTcp = 0,
	Relay = 1,
};

struct TransportConfig {
	TransportKind kind = TransportKind::DirectTcp;
	char relayHost[64] = {};
	uint16_t relayPort = 0;
};

// Establishes `sock` per `cfg`. `host`/`port` are the peer's advertised join
// address. Returns false (and leaves `sock` unconnected) on failure; check
// lastNetError() for DirectTcp, or the session status log for Relay.
bool transportConnect(const TransportConfig& cfg, const char* host, uint16_t port, TcpSocket& sock);

} // namespace toy::net

#include "net/transport.h"

#include "app/session_log.h"

namespace toy::net {

bool transportConnect(const TransportConfig& cfg, const char* host, uint16_t port, TcpSocket& sock)
{
	switch (cfg.kind) {
	case TransportKind::DirectTcp:
		return sock.connect(host, port);
	case TransportKind::Relay:
		// No relay backend exists yet — fail loudly instead of silently
		// falling back to a direct dial the caller didn't ask for.
		toy::sessionLogf("WARN", "relay transport requested (relay=%s:%u) but no relay backend is "
									 "configured in this build; see docs/WAN_PLAY.md",
						  cfg.relayHost[0] ? cfg.relayHost : "(unset)", static_cast<unsigned>(cfg.relayPort));
		return false;
	}
	return false;
}

} // namespace toy::net

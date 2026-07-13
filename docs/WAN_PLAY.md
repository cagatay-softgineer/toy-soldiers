# Playing over the internet (#89)

Toy Soldiers is LAN-first: hosting opens a plain TCP listener and a UDP beacon
for same-network discovery (see [PROTOCOL.md](PROTOCOL.md)). Playing across the
internet works today with a manual step, and there's a code seam for a future
relay backend that would remove it.

## Today: direct connect + port forward

1. The host forwards their TCP port (default `27123`, shown in the lobby as
   "Hosting lobby on port N") on their router to their PC's LAN IP.
2. The host shares their **public** IP (not the LAN IP shown in the room code
   panel) and the port with the joining player, out of band (voice chat, text).
3. The joining player uses **Join** on the main menu and enters that IP:port
   directly — the same join path used for LAN games, just with a public
   address instead of a `192.168.x.x` one.
4. `Settings → recent hosts` remembers the last 5 `name|ip|port` entries
   (`#86/#110`), so reconnecting to the same friend doesn't require retyping.

This has no code dependency — it's the existing `DirectTcp` transport
(`src/net/transport.h`) dialing a public address instead of a private one.
Caveats: symmetric NAT or CGNAT on the host's side will block inbound
connections no matter what; the host needs a router they can configure, or an
existing port-forward/VPN (Tailscale, ZeroTier, Hamachi) bridging both sides
onto the same virtual LAN, which also works with zero code changes since it
just presents as another reachable IP:port.

## Future: relay transport

`src/net/transport.h` defines the seam a relay backend (Steam Networking
Sockets, or a small self-hosted rendezvous/relay service) would plug into:

```cpp
enum class TransportKind : uint8_t { DirectTcp = 0, Relay = 1 };
struct TransportConfig { TransportKind kind; char relayHost[64]; uint16_t relayPort; };
bool transportConnect(const TransportConfig& cfg, const char* host, uint16_t port, TcpSocket& sock);
```

`NetSession::joinLobby` already calls `transportConnect()` rather than dialing
`clientSock_` directly, so once a relay is available, wiring it in is a single
new branch in `transportConnect()` (`src/net/transport.cpp`) that hands back an
already-connected `TcpSocket` obtained via the relay instead of a direct
`connect()` — no other call site changes, since `FrameCodec`/`SendQueue`/the
protocol layer only ever see a connected byte stream.

Selecting `TransportKind::Relay` today fails immediately with a logged reason
("no relay backend is configured in this build") rather than silently falling
back to a direct dial — there is no relay service running anywhere for this
project, so nothing should pretend otherwise. Implementing a real relay branch
needs, at minimum: a running relay/rendezvous endpoint, a way to exchange a
join code through it (the existing room-code system already generates one),
and a hole-punching or always-relayed data path depending on the backend
chosen. That's out of scope until an actual relay service exists to build
against.

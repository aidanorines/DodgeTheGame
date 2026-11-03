#include "server/server.h"

#include <enet/enet.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include "server/lan.h"
#include "server/party.h"
#include "server/registry.h"
#include "shared/names.h"

namespace dodge {
namespace {

using Clock = std::chrono::steady_clock;

constexpr size_t kOnlinePeers = 256;
constexpr size_t kLocalPeers = 8;
constexpr size_t kMaxPacket = 16 * 1024;  // a full Snapshot is about 10 KB
constexpr auto kHelloTimeout = std::chrono::seconds(5);
constexpr uint8_t kReliableChannel = 0;
constexpr uint8_t kFastChannel = 1;

void send(ENetPeer* peer, const Message& m, bool reliable = true) {
  const auto bytes = encode(m);
  ENetPacket* packet = enet_packet_create(bytes.data(), bytes.size(),
                                          reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
  if (enet_peer_send(peer, reliable ? kReliableChannel : kFastChannel, packet) < 0) enet_packet_destroy(packet);
}

RejectReason toReason(JoinResult r) {
  switch (r) {
    case JoinResult::Full: return RejectReason::Full;
    case JoinResult::Started: return RejectReason::Started;
    default: return RejectReason::BadName;
  }
}

struct Conn {
  PeerId id = 0;
  Clock::time_point connectedAt;
  bool helloDone = false;
  std::string name;
  Party* party = nullptr;
};

// What each party's members were last told, so sync() sends only changes.
struct Track {
  std::string code;
  Phase phase = Phase::Lobby;
  std::vector<LobbyPlayer> lobby;
  bool lobbySent = false;
  int ticks = 0;
};

class Server {
 public:
  explicit Server(const ServerConfig& cfg) : cfg_(cfg), registry_(std::random_device{}()) {
    if (cfg.mode != Mode::Online) {
      local_.emplace(cfg.mode, std::random_device{}());
      tracks_[&*local_];
    }
    if (cfg.mode == Mode::Lan) announcer_.emplace();
  }

  void tick(float dt) {
    if (local_) local_->tick(dt);
    registry_.tick(dt);
  }

  void handle(const ENetEvent& e) {
    switch (e.type) {
      case ENET_EVENT_TYPE_CONNECT:
        conns_[e.peer] = {nextPeerId_++, Clock::now()};
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        if (auto it = conns_.find(e.peer); it != conns_.end())
          if (auto m = decode({e.packet->data, e.packet->dataLength})) onMessage(e.peer, it->second, *m);
        enet_packet_destroy(e.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        if (auto it = conns_.find(e.peer); it != conns_.end()) {
          if (it->second.party) it->second.party->leave(it->second.id);
          conns_.erase(it);
        }
        break;
      default:
        break;
    }
  }

  // Runs after events and before the next tick, so closed parties are still alive here.
  void sync() {
    kickSilentPeers();
    announce();
    for (auto it = tracks_.begin(); it != tracks_.end();) {
      syncParty(*it->first, it->second);
      it = it->first->phase() == Phase::Closed ? tracks_.erase(it) : std::next(it);
    }
  }

  bool finished() const { return local_ && local_->phase() == Phase::Closed; }

 private:
  void onMessage(ENetPeer* peer, Conn& c, const Message& m) {
    if (!c.helloDone) {
      if (auto* hello = std::get_if<Hello>(&m)) onHello(peer, c, *hello);
      return;
    }
    if (!c.party) {
      if (std::holds_alternative<CreateParty>(m) && cfg_.mode == Mode::Online) {
        const auto code = registry_.create();
        enter(peer, c, *registry_.find(code), code);
      } else if (auto* join = std::get_if<JoinParty>(&m); join && cfg_.mode == Mode::Online) {
        if (Party* p = registry_.find(join->code))
          enter(peer, c, *p, tracks_[p].code);
        else
          send(peer, Reject{RejectReason::NoSuchParty});
      }
      return;
    }
    if (auto* ready = std::get_if<SetReady>(&m)) {
      c.party->setReady(c.id, ready->ready);
    } else if (auto* in = std::get_if<InputMsg>(&m)) {
      c.party->setInput(c.id, in->input);
    } else if (std::holds_alternative<Leave>(m)) {
      c.party->leave(c.id);
      c.party = nullptr;
      enet_peer_disconnect_later(peer, 0);
    }
  }

  void onHello(ENetPeer* peer, Conn& c, const Hello& hello) {
    if (hello.version != kProtocolVersion || hello.mode != cfg_.mode) {
      refuse(peer, RejectReason::Version);
      return;
    }
    if (cfg_.mode != Mode::Solo && !sanitizeName(hello.name)) {
      refuse(peer, RejectReason::BadName);
      return;
    }
    c.helloDone = true;
    c.name = hello.name;
    if (local_ && !enter(peer, c, *local_, "")) enet_peer_disconnect_later(peer, 0);
  }

  void refuse(ENetPeer* peer, RejectReason reason) {
    send(peer, Reject{reason});
    enet_peer_disconnect_later(peer, 0);
  }

  bool enter(ENetPeer* peer, Conn& c, Party& party, const std::string& code) {
    const JoinResult result = party.join(c.id, c.name);
    if (result != JoinResult::Ok) {
      send(peer, Reject{toReason(result)});
      return false;
    }
    c.party = &party;
    tracks_[&party].code = code;
    send(peer, Welcome{*party.playerId(c.id)});
    return true;
  }

  // Started matches leave the list: they cannot be joined.
  void announce() {
    if (!announcer_ || ++announceTicks_ % kTickHz != 1) return;
    if (local_->phase() != Phase::Lobby && local_->phase() != Phase::Countdown) return;
    announcer_->announce({cfg_.lanName, 0, cfg_.port, static_cast<uint8_t>(local_->lobbyPlayers().size())});
  }

  void kickSilentPeers() {
    const auto now = Clock::now();
    for (auto& [peer, c] : conns_)
      if (!c.helloDone && now - c.connectedAt > kHelloTimeout) enet_peer_disconnect(peer, 0);
  }

  template <class Fn>
  void forMembers(const Party& party, Fn fn) {
    for (auto& [peer, c] : conns_)
      if (c.party == &party) fn(peer, c);
  }

  void broadcast(const Party& party, const Message& m, bool reliable = true) {
    forMembers(party, [&](ENetPeer* peer, Conn&) { send(peer, m, reliable); });
  }

  void syncParty(Party& party, Track& t) {
    const Phase phase = party.phase();
    const bool phaseChanged = phase != t.phase;
    t.phase = phase;

    if (phase == Phase::Lobby || phase == Phase::Countdown) {
      auto lobby = party.lobbyPlayers();
      if (phaseChanged || !t.lobbySent || lobby != t.lobby) {
        t.lobby = std::move(lobby);
        t.lobbySent = true;
        broadcast(party, LobbyState{t.code, t.lobby, party.countdown()});
      }
    } else if (phase == Phase::Closed) {
      broadcast(party, PartyClosed{});
      forMembers(party, [](ENetPeer* peer, Conn& c) {
        c.party = nullptr;
        enet_peer_disconnect_later(peer, 0);
      });
      return;
    }

    if (phaseChanged && phase == Phase::Match) broadcast(party, MatchStart{0});
    if (phaseChanged && phase == Phase::Results) broadcast(party, MatchEnd{party.results(), party.countdown()});

    if (const World* w = party.world(); w && ++t.ticks % (kTickHz / kSnapshotHz) == 0)
      broadcast(party, Snapshot{w->tick, w->players, w->hazards}, false);
  }

  ServerConfig cfg_;
  PartyRegistry registry_;
  std::optional<Party> local_;  // the one party in solo and LAN modes
  std::optional<LanAnnouncer> announcer_;
  int announceTicks_ = 0;
  std::map<ENetPeer*, Conn> conns_;
  std::map<Party*, Track> tracks_;
  PeerId nextPeerId_ = 1;
};

// Lets queued PartyClosed and disconnect packets leave before the host is destroyed.
void drain(ENetHost* host) {
  ENetEvent e;
  const auto end = Clock::now() + std::chrono::milliseconds(200);
  while (Clock::now() < end)
    if (enet_host_service(host, &e, 10) > 0 && e.type == ENET_EVENT_TYPE_RECEIVE) enet_packet_destroy(e.packet);
}

}  // namespace

void runServer(const ServerConfig& cfg, std::atomic<bool>& stop) {
  if (enet_initialize() != 0) {
    std::fprintf(stderr, "dodge: enet_initialize failed\n");
    return;
  }
  ENetAddress address{};
  address.host = ENET_HOST_ANY;
  address.port = cfg.port;
  ENetHost* host = enet_host_create(&address, cfg.mode == Mode::Online ? kOnlinePeers : kLocalPeers, 2, 0, 0);
  if (!host) {
    std::fprintf(stderr, "dodge: cannot listen on UDP port %u\n", static_cast<unsigned>(cfg.port));
    enet_deinitialize();
    return;
  }
  host->maximumPacketSize = kMaxPacket;

  Server server(cfg);
  const auto tickLen = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / kTickHz));
  auto next = Clock::now() + tickLen;

  while (!stop && !server.finished()) {
    server.tick(1.0f / kTickHz);

    for (;;) {
      const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(next - Clock::now()).count();
      ENetEvent e;
      const int got = enet_host_service(host, &e, static_cast<enet_uint32>(std::max<long long>(left, 0)));
      if (got > 0) server.handle(e);
      if (got < 0) std::fprintf(stderr, "dodge: enet_host_service failed\n");
      if (got <= 0 && Clock::now() >= next) break;
      if (got > 0 && Clock::now() >= next + tickLen) break;  // never let a packet flood starve ticks
    }

    server.sync();
    next += tickLen;
    if (Clock::now() - next > std::chrono::milliseconds(250)) next = Clock::now();  // after a stall, do not fast-forward
  }

  enet_host_flush(host);
  drain(host);
  enet_host_destroy(host);
  enet_deinitialize();
}

}  // namespace dodge

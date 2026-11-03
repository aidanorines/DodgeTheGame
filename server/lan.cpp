#include "server/lan.h"

#include <enet/enet.h>

#include <algorithm>
#include <cstring>

#include "shared/names.h"

namespace dodge {
namespace {

constexpr char kMagic[4] = {'D', 'O', 'D', 'G'};
constexpr size_t kHeaderSize = 10;  // magic(4) version(2) port(2) players(1) nameLen(1)
constexpr double kStaleSeconds = 3.0;

ENetSocket openSocket() {
  ENetSocket s = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (s == ENET_SOCKET_NULL) return s;
  enet_socket_set_option(s, ENET_SOCKOPT_NONBLOCK, 1);
  enet_socket_set_option(s, ENET_SOCKOPT_BROADCAST, 1);
  enet_socket_set_option(s, ENET_SOCKOPT_REUSEADDR, 1);
  return s;
}

ENetSocket toSocket(uintptr_t s) { return static_cast<ENetSocket>(s); }

}  // namespace

std::vector<uint8_t> encodeAnnounce(const LanGame& g) {
  std::vector<uint8_t> out(kMagic, kMagic + sizeof kMagic);
  const uint16_t version = kProtocolVersion;
  out.push_back(static_cast<uint8_t>(version));
  out.push_back(static_cast<uint8_t>(version >> 8));
  out.push_back(static_cast<uint8_t>(g.port));
  out.push_back(static_cast<uint8_t>(g.port >> 8));
  out.push_back(g.players);
  out.push_back(static_cast<uint8_t>(g.hostName.size()));
  out.insert(out.end(), g.hostName.begin(), g.hostName.end());
  return out;
}

std::optional<LanGame> parseAnnounce(std::span<const uint8_t> b, uint32_t ip) {
  if (b.size() < kHeaderSize || std::memcmp(b.data(), kMagic, sizeof kMagic) != 0) return std::nullopt;
  if ((b[4] | b[5] << 8) != kProtocolVersion) return std::nullopt;
  const uint8_t players = b[8];
  const size_t nameLen = b[9];
  if (players > kMaxPlayers || b.size() != kHeaderSize + nameLen) return std::nullopt;

  const std::string_view raw(reinterpret_cast<const char*>(b.data() + kHeaderSize), nameLen);
  auto name = sanitizeName(raw);
  if (!name) return std::nullopt;
  return LanGame{*name, ip, static_cast<uint16_t>(b[6] | b[7] << 8), players};
}

LanAnnouncer::LanAnnouncer(uint16_t discoveryPort, uint32_t target)
    : socket_(static_cast<uintptr_t>(openSocket())), port_(discoveryPort), target_(target) {}

LanAnnouncer::~LanAnnouncer() {
  if (toSocket(socket_) != ENET_SOCKET_NULL) enet_socket_destroy(toSocket(socket_));
}

void LanAnnouncer::announce(const LanGame& g) {
  if (toSocket(socket_) == ENET_SOCKET_NULL) return;
  auto bytes = encodeAnnounce(g);
  ENetAddress to{};
  to.host = target_;
  to.port = port_;
  ENetBuffer buf{};
  buf.data = bytes.data();
  buf.dataLength = bytes.size();
  enet_socket_send(toSocket(socket_), &to, &buf, 1);
}

LanBrowser::LanBrowser(uint16_t discoveryPort) : socket_(static_cast<uintptr_t>(openSocket())) {
  if (toSocket(socket_) == ENET_SOCKET_NULL) return;
  ENetAddress bindTo{};
  bindTo.host = ENET_HOST_ANY;
  bindTo.port = discoveryPort;
  enet_socket_bind(toSocket(socket_), &bindTo);
}

LanBrowser::~LanBrowser() {
  if (toSocket(socket_) != ENET_SOCKET_NULL) enet_socket_destroy(toSocket(socket_));
}

void LanBrowser::poll(double now) {
  if (toSocket(socket_) != ENET_SOCKET_NULL) {
    uint8_t data[256];
    for (;;) {
      ENetAddress from{};
      ENetBuffer buf{};
      buf.data = data;
      buf.dataLength = sizeof data;
      const int got = enet_socket_receive(toSocket(socket_), &from, &buf, 1);
      if (got <= 0) break;
      auto game = parseAnnounce(std::span<const uint8_t>(data, static_cast<size_t>(got)), from.host);
      if (!game) continue;
      auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) {
        return e.game.ip == game->ip && e.game.port == game->port;
      });
      if (it == entries_.end())
        entries_.push_back({*game, now});
      else
        *it = {*game, now};
    }
  }
  std::erase_if(entries_, [&](const Entry& e) { return now - e.lastSeen > kStaleSeconds; });
}

std::vector<LanGame> LanBrowser::games() const {
  std::vector<LanGame> out;
  for (const auto& e : entries_) out.push_back(e.game);
  return out;
}

}  // namespace dodge

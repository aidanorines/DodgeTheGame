#include <doctest/doctest.h>
#include <enet/enet.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#include "server/server.h"
#include "shared/protocol.h"

using namespace dodge;

namespace {

// Minimal raw ENet client, so these tests exercise the real wire format.
struct TestClient {
  ENetHost* host = nullptr;
  ENetPeer* peer = nullptr;
  std::vector<Message> inbox;
  bool connected = false;
  bool disconnected = false;

  explicit TestClient(uint16_t port) {
    enet_initialize();
    host = enet_host_create(nullptr, 1, 2, 0, 0);
    ENetAddress addr{};
    enet_address_set_host(&addr, "127.0.0.1");
    addr.port = port;
    peer = enet_host_connect(host, &addr, 2, 0);
    for (int i = 0; i < 200 && !connected; ++i) pump(10);
  }

  ~TestClient() {
    enet_peer_reset(peer);
    enet_host_destroy(host);
    enet_deinitialize();
  }

  void pump(uint32_t ms) {
    ENetEvent e;
    while (enet_host_service(host, &e, ms) > 0) {
      ms = 0;
      if (e.type == ENET_EVENT_TYPE_CONNECT) connected = true;
      if (e.type == ENET_EVENT_TYPE_DISCONNECT) disconnected = true;
      if (e.type == ENET_EVENT_TYPE_RECEIVE) {
        if (auto m = decode({e.packet->data, e.packet->dataLength})) inbox.push_back(*m);
        enet_packet_destroy(e.packet);
      }
    }
  }

  void sendRaw(const std::vector<uint8_t>& bytes) {
    enet_peer_send(peer, 0, enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE));
    enet_host_flush(host);
  }

  void send(const Message& m) { sendRaw(encode(m)); }

  // First queued T that passes pred; everything before it is consumed.
  template <class T, class Pred>
  std::optional<T> waitFor(int timeoutMs, Pred pred) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < end) {
      pump(5);
      for (size_t i = 0; i < inbox.size(); ++i) {
        if (const T* t = std::get_if<T>(&inbox[i]); t && pred(*t)) {
          T out = *t;
          inbox.erase(inbox.begin(), inbox.begin() + static_cast<long>(i) + 1);
          return out;
        }
      }
    }
    return std::nullopt;
  }

  template <class T>
  std::optional<T> waitFor(int timeoutMs) {
    return waitFor<T>(timeoutMs, [](const T&) { return true; });
  }
};

// Runs a server on a thread for the length of a test.
struct TestServer {
  std::atomic<bool> stop{false};
  std::thread thread;
  explicit TestServer(ServerConfig cfg) : thread([this, cfg] { runServer(cfg, stop); }) {}
  ~TestServer() {
    stop = true;
    thread.join();
  }
};

constexpr auto kPlayers2 = [](const LobbyState& s) { return s.players.size() == 2; };

}  // namespace

TEST_CASE("loopback lobby") {
  TestServer server({Mode::Online, 17777, ""});

  TestClient a(17777);
  REQUIRE(a.connected);
  a.send(Hello{kProtocolVersion, Mode::Online, "Ann"});
  a.send(CreateParty{});
  REQUIRE(a.waitFor<Welcome>(2000));
  auto lobby = a.waitFor<LobbyState>(2000);
  REQUIRE(lobby);
  CHECK(lobby->code.size() == static_cast<size_t>(kCodeLen));

  TestClient b(17777);
  REQUIRE(b.connected);
  b.send(Hello{kProtocolVersion, Mode::Online, "Bo"});
  b.send(JoinParty{lobby->code});
  auto bLobby = b.waitFor<LobbyState>(2000, kPlayers2);
  REQUIRE(bLobby);
  CHECK(bLobby->players[1].name == "Bo");
  CHECK(a.waitFor<LobbyState>(2000, kPlayers2));

  TestClient old(17777);
  REQUIRE(old.connected);
  old.send(Hello{99, Mode::Online, "Old"});
  auto rej = old.waitFor<Reject>(2000);
  REQUIRE(rej);
  CHECK(rej->reason == RejectReason::Version);
}

TEST_CASE("bad party code can be retried") {
  TestServer server({Mode::Online, 17781, ""});
  TestClient a(17781);
  REQUIRE(a.connected);
  a.send(Hello{kProtocolVersion, Mode::Online, "Ann"});
  a.send(JoinParty{"00000"});
  auto rej = a.waitFor<Reject>(2000);
  REQUIRE(rej);
  CHECK(rej->reason == RejectReason::NoSuchParty);
  a.send(CreateParty{});
  CHECK(a.waitFor<Welcome>(2000));
}

TEST_CASE("bad name is rejected at hello") {
  TestServer server({Mode::Online, 17782, ""});
  TestClient a(17782);
  REQUIRE(a.connected);
  a.send(Hello{kProtocolVersion, Mode::Online, "bad!"});
  auto rej = a.waitFor<Reject>(2000);
  REQUIRE(rej);
  CHECK(rej->reason == RejectReason::BadName);
}

TEST_CASE("garbage and out-of-order messages are ignored") {
  TestServer server({Mode::Online, 17783, ""});
  TestClient a(17783);
  REQUIRE(a.connected);
  a.sendRaw({255, 1, 2, 3});
  a.sendRaw(std::vector<uint8_t>(3000, 0xAB));
  a.send(SetReady{true});   // before Hello
  a.send(CreateParty{});    // before Hello
  a.send(Hello{kProtocolVersion, Mode::Online, "Ann"});
  a.send(SetReady{true});   // not in a party yet
  a.send(CreateParty{});
  CHECK(a.waitFor<Welcome>(2000));
}

TEST_CASE("leaving updates the others") {
  TestServer server({Mode::Online, 17784, ""});
  TestClient a(17784);
  REQUIRE(a.connected);
  a.send(Hello{kProtocolVersion, Mode::Online, "Ann"});
  a.send(CreateParty{});
  auto lobby = a.waitFor<LobbyState>(2000);
  REQUIRE(lobby);
  {
    TestClient b(17784);
    REQUIRE(b.connected);
    b.send(Hello{kProtocolVersion, Mode::Online, "Bo"});
    b.send(JoinParty{lobby->code});
    REQUIRE(a.waitFor<LobbyState>(2000, kPlayers2));
    b.send(Leave{});
    b.pump(50);
  }
  CHECK(a.waitFor<LobbyState>(2000, [](const LobbyState& s) { return s.players.size() == 1; }));
}

TEST_CASE("stopping the server closes parties") {
  std::atomic<bool> stop{false};
  std::thread thread([&] { runServer({Mode::Lan, 17785, "Host"}, stop); });
  TestClient a(17785);
  REQUIRE(a.connected);
  a.send(Hello{kProtocolVersion, Mode::Lan, "Host"});
  REQUIRE(a.waitFor<Welcome>(2000));
  stop = true;
  CHECK(a.waitFor<PartyClosed>(2000));
  thread.join();
}

TEST_CASE("solo plays through and the server exits") {
  std::atomic<bool> stop{false};
  std::thread thread([&] { runServer({Mode::Solo, 17780, ""}, stop); });
  {
    TestClient c(17780);
    REQUIRE(c.connected);
    c.send(Hello{kProtocolVersion, Mode::Solo, ""});
    REQUIRE(c.waitFor<Welcome>(2000));
    auto lobby = c.waitFor<LobbyState>(2000);
    REQUIRE(lobby);
    CHECK(lobby->countdown > 0);
    REQUIRE(c.waitFor<MatchStart>(5000));
    REQUIRE(c.waitFor<Snapshot>(1000));

    const std::vector<uint8_t> left = encode(InputMsg{{-1, 0}});
    enet_peer_send(c.peer, 1, enet_packet_create(left.data(), left.size(), ENET_PACKET_FLAG_UNSEQUENCED));
    enet_host_flush(c.host);

    auto end = c.waitFor<MatchEnd>(5000);
    REQUIRE(end);
    REQUIRE(end->results.size() == 1);
    CHECK(end->results[0].place == 1);
    CHECK(c.waitFor<PartyClosed>(7000));
  }
  thread.join();  // exits on its own: hangs here if the solo loop does not stop
}

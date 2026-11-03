#include <doctest/doctest.h>
#include <enet/enet.h>

#include <chrono>
#include <thread>

#include "server/lan.h"
#include "shared/constants.h"

using namespace dodge;

namespace {

constexpr uint16_t kTestPort = 17778;

uint32_t loopback() {
  ENetAddress a{};
  enet_address_set_host_ip(&a, "127.0.0.1");
  return a.host;
}

// Datagrams on loopback land almost at once, but poll a few times to be safe.
void pollUntilSeen(LanBrowser& b, double now) {
  for (int i = 0; i < 50 && b.games().empty(); ++i) {
    b.poll(now);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

}  // namespace

TEST_CASE("announce packet round trip") {
  const auto bytes = encodeAnnounce({"Ann", 0, 7777, 2});
  const auto game = parseAnnounce(bytes, 1234);
  REQUIRE(game);
  CHECK(game->hostName == "Ann");
  CHECK(game->ip == 1234);
  CHECK(game->port == 7777);
  CHECK(game->players == 2);
}

TEST_CASE("bad announce packets are ignored") {
  auto good = encodeAnnounce({"Ann", 0, 7777, 2});

  auto badMagic = good;
  badMagic[0] = 'X';
  CHECK_FALSE(parseAnnounce(badMagic, 1));

  auto badVersion = good;
  badVersion[4] = static_cast<uint8_t>(kProtocolVersion + 1);
  CHECK_FALSE(parseAnnounce(badVersion, 1));

  auto tooMany = good;
  tooMany[8] = kMaxPlayers + 1;
  CHECK_FALSE(parseAnnounce(tooMany, 1));

  for (size_t len = 0; len < good.size(); ++len)
    CHECK_FALSE(parseAnnounce(std::span(good.data(), len), 1));

  CHECK_FALSE(parseAnnounce(encodeAnnounce({"bad!", 0, 7777, 2}), 1));
}

TEST_CASE("announce is seen and stale games expire") {
  enet_initialize();
  {
    LanBrowser browser(kTestPort);
    LanAnnouncer announcer(kTestPort, loopback());

    announcer.announce({"Ann", 0, 7777, 2});
    announcer.announce({"Ann", 0, 7777, 3});  // same host updates its row
    pollUntilSeen(browser, 10.0);
    browser.poll(10.0);
    REQUIRE(browser.games().size() == 1);
    CHECK(browser.games()[0].hostName == "Ann");
    CHECK(browser.games()[0].port == 7777);
    CHECK(browser.games()[0].ip == loopback());

    browser.poll(12.9);
    CHECK(browser.games().size() == 1);
    browser.poll(14.0);
    CHECK(browser.games().empty());
  }
  enet_deinitialize();
}

#include <doctest/doctest.h>

#include "shared/protocol.h"

using namespace dodge;

static void roundTrip(const Message& m) {
  const auto bytes = encode(m);
  const auto back = decode(bytes);
  REQUIRE(back.has_value());
  CHECK(*back == m);
}

static Snapshot sampleSnapshot() {
  return {77, {{1, {10.5f, 20}, true, 3.25f, 0}, {2, {-4, 700}, false, 1.5f, 90}},
          {{5, {0, 1}, {250, 0}}, {6, {1280, 9}, {0, -250}}}};
}

TEST_CASE("round trip") {
  roundTrip(Hello{kProtocolVersion, Mode::Online, "Ann"});
  roundTrip(CreateParty{});
  roundTrip(JoinParty{"ABCDE"});
  roundTrip(JoinParty{""});
  roundTrip(SetReady{true});
  roundTrip(InputMsg{{-1, 0.5f}});
  roundTrip(Leave{});
  roundTrip(Welcome{3});
  roundTrip(Reject{RejectReason::NoSuchParty});
  roundTrip(LobbyState{"XYZ23", {{1, 0, "Ann", true}, {2, 3, "Bo", false}}, -1});
  roundTrip(MatchStart{3});
  roundTrip(sampleSnapshot());
  roundTrip(MatchEnd{{{2, 1, 9.5f}, {1, 2, 4}}, 5});
  roundTrip(PartyClosed{});
}

TEST_CASE("rejects garbage") {
  CHECK_FALSE(decode({}));
  const std::vector<uint8_t> badTag{255};
  CHECK_FALSE(decode(badTag));

  const auto snap = encode(sampleSnapshot());
  for (size_t len = 0; len < snap.size(); ++len)
    CHECK_FALSE(decode(std::span(snap.data(), len)));

  auto hello = encode(Hello{kProtocolVersion, Mode::Lan, "Ann"});
  hello.push_back(0);
  CHECK_FALSE(decode(hello));
}

TEST_CASE("rejects out-of-range enums and bools") {
  auto hello = encode(Hello{kProtocolVersion, Mode::Lan, ""});
  hello[3] = 9;  // tag(1) + version(2), then mode
  CHECK_FALSE(decode(hello));

  auto ready = encode(SetReady{true});
  ready[1] = 2;
  CHECK_FALSE(decode(ready));
}

TEST_CASE("rejects oversized strings") {
  std::vector<uint8_t> join{static_cast<uint8_t>(Message(JoinParty{}).index()), 65};
  join.resize(join.size() + 65, 'A');
  CHECK_FALSE(decode(join));
}

TEST_CASE("rejects huge counts") {
  // Snapshot: tag, tick(4), players count(2) = 0, hazards count(2) = 60000, no payload.
  const std::vector<uint8_t> buf{static_cast<uint8_t>(Message(Snapshot{}).index()), 0, 0, 0, 0, 0, 0, 0x60, 0xEA};
  CHECK_FALSE(decode(buf));
}

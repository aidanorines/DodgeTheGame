#include <doctest/doctest.h>

#include <set>

#include "server/party.h"
#include "shared/constants.h"

using namespace dodge;

static void readyAll(Party& p, std::initializer_list<PeerId> peers) {
  for (PeerId peer : peers) p.setReady(peer, true);
}

// Run the lobby countdown to completion.
static void startMatch(Party& p) { p.tick(kLobbyCountdown + 0.01f); }

TEST_CASE("fifth join is full") {
  Party p(Mode::Online, 1);
  for (PeerId i = 1; i <= 4; ++i) CHECK(p.join(i, "P") == JoinResult::Ok);
  CHECK(p.join(5, "P") == JoinResult::Full);
}

TEST_CASE("join during match is rejected") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  readyAll(p, {1, 2});
  startMatch(p);
  REQUIRE(p.phase() == Phase::Match);
  CHECK(p.join(3, "C") == JoinResult::Started);
}

TEST_CASE("bad name rejected") {
  Party p(Mode::Lan, 1);
  CHECK(p.join(1, "bad!") == JoinResult::BadName);
  CHECK(p.join(2, "caf\xC3\xA9") == JoinResult::BadName);
  CHECK(p.lobbyPlayers().empty());
}

TEST_CASE("colors are distinct and reused") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  p.join(3, "C");
  std::set<uint8_t> colors;
  for (const auto& lp : p.lobbyPlayers()) colors.insert(lp.color);
  CHECK(colors == std::set<uint8_t>{0, 1, 2});
  p.leave(2);
  p.join(4, "D");
  CHECK(p.lobbyPlayers().back().color == 1);
}

TEST_CASE("solo starts countdown on join") {
  Party p(Mode::Solo, 1);
  CHECK(p.join(1, "") == JoinResult::Ok);
  CHECK(p.phase() == Phase::Countdown);
  CHECK(p.countdown() == doctest::Approx(kSoloCountdown));
  CHECK(p.join(2, "") == JoinResult::Full);
}

TEST_CASE("all ready with 2 starts countdown") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  p.setReady(1, true);
  CHECK(p.phase() == Phase::Lobby);
  CHECK(p.countdown() < 0);
  p.setReady(2, true);
  CHECK(p.phase() == Phase::Countdown);
  CHECK(p.countdown() == doctest::Approx(kLobbyCountdown));
}

TEST_CASE("one player all ready does not start") {
  Party p(Mode::Lan, 1);
  p.join(1, "A");
  p.setReady(1, true);
  CHECK(p.phase() == Phase::Lobby);
}

TEST_CASE("unready cancels countdown") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  readyAll(p, {1, 2});
  p.setReady(1, false);
  CHECK(p.phase() == Phase::Lobby);
}

TEST_CASE("new join cancels countdown") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  readyAll(p, {1, 2});
  p.join(3, "C");
  CHECK(p.phase() == Phase::Lobby);
}

TEST_CASE("disconnect below 2 cancels countdown") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  readyAll(p, {1, 2});
  p.leave(2);
  CHECK(p.phase() == Phase::Lobby);
}

TEST_CASE("unready player leaving lets the rest start") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  p.join(3, "C");
  readyAll(p, {1, 2});
  p.leave(3);
  CHECK(p.phase() == Phase::Countdown);
}

TEST_CASE("countdown ends into match") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  readyAll(p, {1, 2});
  p.tick(1.0f);
  CHECK(p.phase() == Phase::Countdown);
  CHECK(p.world() == nullptr);
  p.tick(2.01f);
  CHECK(p.phase() == Phase::Match);
  REQUIRE(p.world() != nullptr);
  CHECK(p.world()->players.size() == 2);
}

TEST_CASE("leave in match eliminates") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  readyAll(p, {1, 2});
  startMatch(p);
  const uint8_t id = *p.playerId(2);
  p.leave(2);
  for (const auto& ps : p.world()->players)
    if (ps.id == id) CHECK_FALSE(ps.alive);
  CHECK(p.phase() == Phase::Match);
}

TEST_CASE("all out goes to results then closed") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  readyAll(p, {1, 2});
  startMatch(p);
  for (int i = 0; i < 10 * kTickHz && p.phase() == Phase::Match; ++i) {
    p.setInput(1, {-1, 0});
    p.setInput(2, {1, 0});
    p.tick(1.0f / kTickHz);
  }
  REQUIRE(p.phase() == Phase::Results);
  CHECK(p.countdown() == doctest::Approx(kResultsCountdown));
  CHECK(p.results().size() == 2);
  REQUIRE(p.world() != nullptr);  // spectators keep watching during results
  p.tick(kResultsCountdown - 0.1f);
  CHECK(p.phase() == Phase::Results);
  p.tick(0.2f);
  CHECK(p.phase() == Phase::Closed);
}

TEST_CASE("lan creator leaving closes party") {
  SUBCASE("in lobby") {
    Party p(Mode::Lan, 1);
    p.join(1, "Host");
    p.join(2, "B");
    p.leave(1);
    CHECK(p.phase() == Phase::Closed);
  }
  SUBCASE("in match") {
    Party p(Mode::Lan, 1);
    p.join(1, "Host");
    p.join(2, "B");
    p.join(3, "C");
    readyAll(p, {1, 2, 3});
    startMatch(p);
    p.leave(1);
    CHECK(p.phase() == Phase::Closed);
  }
}

TEST_CASE("lan non-creator leaving keeps party") {
  Party p(Mode::Lan, 1);
  p.join(1, "Host");
  p.join(2, "B");
  p.leave(2);
  CHECK(p.phase() == Phase::Lobby);
}

TEST_CASE("online creator leaving keeps party") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.join(2, "B");
  p.leave(1);
  CHECK(p.phase() == Phase::Lobby);
  CHECK(p.lobbyPlayers().size() == 1);
  p.leave(2);
  CHECK(p.empty());
  CHECK(p.phase() == Phase::Closed);
}

TEST_CASE("unknown peers are ignored") {
  Party p(Mode::Online, 1);
  p.join(1, "A");
  p.setReady(99, true);
  p.setInput(99, {1, 1});
  p.leave(99);
  CHECK(p.lobbyPlayers().size() == 1);
  CHECK_FALSE(p.playerId(99));
}

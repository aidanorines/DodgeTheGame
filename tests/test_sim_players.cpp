#include <doctest/doctest.h>

#include <cmath>

#include "shared/constants.h"
#include "shared/sim.h"

using namespace dodge;

static float dist(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

TEST_CASE("diagonal is normalized") {
  World w = makeWorld({1}, 42);
  const Vec2 start = w.players[0].pos;
  stepWorld(w, {{1, {1, 1}}}, 1.0f);
  CHECK(dist(start, w.players[0].pos) == doctest::Approx(kPlayerSpeed));
}

TEST_CASE("input is clamped") {
  World w = makeWorld({1}, 42);
  const Vec2 start = w.players[0].pos;
  stepWorld(w, {{1, {50, 0}}}, 1.0f);
  CHECK(dist(start, w.players[0].pos) == doctest::Approx(kPlayerSpeed));
}

TEST_CASE("non-finite input is ignored") {
  World w = makeWorld({1}, 42);
  const Vec2 start = w.players[0].pos;
  stepWorld(w, {{1, {NAN, INFINITY}}}, 1.0f);
  CHECK(w.players[0].pos.x == start.x);
  CHECK(w.players[0].pos.y == start.y);
}

TEST_CASE("leaving arena eliminates") {
  World w = makeWorld({1}, 42);
  w.players[0].pos = {5, 360};
  stepWorld(w, {{1, {-1, 0}}}, 0.1f);
  CHECK_FALSE(w.players[0].alive);
  CHECK(allEliminated(w));
}

TEST_CASE("dead players do not move") {
  World w = makeWorld({1}, 42);
  w.players[0].pos = {5, 360};
  stepWorld(w, {{1, {-1, 0}}}, 0.1f);
  const Vec2 out = w.players[0].pos;
  stepWorld(w, {{1, {1, 0}}}, 0.1f);
  CHECK(w.players[0].pos.x == out.x);
  CHECK(w.players[0].pos.y == out.y);
}

TEST_CASE("players start in a grid around the center") {
  World w = makeWorld({1, 2, 3, 4}, 42);
  REQUIRE(w.players.size() == 4);
  for (const auto& p : w.players) {
    CHECK(p.alive);
    CHECK(std::abs(p.pos.x - kArenaW / 2) == doctest::Approx(30));
    CHECK(std::abs(p.pos.y - kArenaH / 2) == doctest::Approx(30));
  }
}

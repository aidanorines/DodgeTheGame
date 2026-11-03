#include <doctest/doctest.h>

#include <cmath>
#include <set>

#include "shared/constants.h"
#include "shared/sim.h"

using namespace dodge;

TEST_CASE("spawn interval curve") {
  CHECK(spawnInterval(0) == doctest::Approx(1.0));
  CHECK(spawnInterval(50) == doctest::Approx(0.5));
  CHECK(spawnInterval(1000) == doctest::Approx(0.15));
}

TEST_CASE("hazards start on an edge moving inward") {
  std::mt19937 rng(7);
  std::set<int> edges;
  for (uint32_t i = 0; i < 1000; ++i) {
    const Hazard h = spawnHazard(rng, i);
    CHECK(h.id == i);
    CHECK(std::hypot(h.vel.x, h.vel.y) == doctest::Approx(kHazardSpeed));
    if (h.pos.x == -kHazardRadius) {
      edges.insert(0);
      CHECK(h.vel.x > 0);
    } else if (h.pos.x == kArenaW + kHazardRadius) {
      edges.insert(1);
      CHECK(h.vel.x < 0);
    } else if (h.pos.y == -kHazardRadius) {
      edges.insert(2);
      CHECK(h.vel.y > 0);
    } else if (h.pos.y == kArenaH + kHazardRadius) {
      edges.insert(3);
      CHECK(h.vel.y < 0);
    } else {
      FAIL("hazard not on an edge");
    }
  }
  CHECK(edges.size() == 4);
}

TEST_CASE("hazards are culled off-screen") {
  World w = makeWorld({}, 1);
  w.spawnTimer = 100;  // no new spawns
  w.hazards.push_back({0, {kArenaW + 13, 100}, {kHazardSpeed, 0}});
  stepWorld(w, {}, 1.0f / kTickHz);
  CHECK(w.hazards.empty());
}

TEST_CASE("hazards move in a straight line") {
  World w = makeWorld({}, 1);
  w.spawnTimer = 100;
  w.hazards.push_back({0, {100, 100}, {0, kHazardSpeed}});
  stepWorld(w, {}, 1.0f);
  REQUIRE(w.hazards.size() == 1);
  CHECK(w.hazards[0].pos.x == doctest::Approx(100));
  CHECK(w.hazards[0].pos.y == doctest::Approx(100 + kHazardSpeed));
}

TEST_CASE("hazard hit eliminates") {
  World w = makeWorld({1}, 1);
  w.spawnTimer = 100;
  w.hazards.push_back({0, w.players[0].pos, {0, 0}});
  stepWorld(w, {}, 1.0f / kTickHz);
  CHECK_FALSE(w.players[0].alive);
}

TEST_CASE("spawn rate rises over time") {
  World w = makeWorld({}, 3);
  uint32_t early = 0, late = 0;
  for (int i = 0; i < 10 * kTickHz; ++i) stepWorld(w, {}, 1.0f / kTickHz);
  early = w.nextHazardId;
  for (int i = 0; i < 80 * kTickHz; ++i) stepWorld(w, {}, 1.0f / kTickHz);
  const uint32_t before = w.nextHazardId;
  for (int i = 0; i < 10 * kTickHz; ++i) stepWorld(w, {}, 1.0f / kTickHz);
  late = w.nextHazardId - before;
  CHECK(late > early * 2);
}

TEST_CASE("same-tick eliminations share placement") {
  World w = makeWorld({1, 2, 3}, 1);
  w.players[0].alive = false;
  w.players[0].outTick = 10;
  w.players[1].alive = false;
  w.players[1].outTick = 20;
  w.players[2].alive = false;
  w.players[2].outTick = 20;
  const auto pl = placements(w);
  REQUIRE(pl.size() == 3);
  CHECK(pl[0].place == 1);
  CHECK(pl[1].place == 1);
  CHECK(pl[2].place == 3);
  CHECK(pl[2].id == 1);
}

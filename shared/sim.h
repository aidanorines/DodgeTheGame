#pragma once

#include <cstdint>
#include <map>
#include <random>
#include <vector>

namespace dodge {

struct Vec2 {
  float x = 0, y = 0;
  bool operator==(const Vec2&) const = default;
};

// Client intent. Untrusted: stepWorld clamps it.
struct Input {
  float dx = 0, dy = 0;
  bool operator==(const Input&) const = default;
};

struct PlayerState {
  uint8_t id = 0;
  Vec2 pos;
  bool alive = true;
  float survived = 0;
  uint32_t outTick = 0;
  bool operator==(const PlayerState&) const = default;
};

struct Hazard {
  uint32_t id = 0;
  Vec2 pos;
  Vec2 vel;
  bool operator==(const Hazard&) const = default;
};

struct World {
  uint32_t tick = 0;
  float time = 0;
  float spawnTimer = 0;
  uint32_t nextHazardId = 0;
  std::vector<PlayerState> players;
  std::vector<Hazard> hazards;
  std::mt19937 rng;
};

struct Placement {
  uint8_t id = 0;
  uint8_t place = 0;
  float survived = 0;
  bool operator==(const Placement&) const = default;
};

World makeWorld(const std::vector<uint8_t>& playerIds, uint32_t seed);
void stepWorld(World& w, const std::map<uint8_t, Input>& inputs, float dt);
bool allEliminated(const World& w);
// Eliminate a player now (e.g. they left). No-op if unknown or already out.
void eliminatePlayer(World& w, uint8_t id);

// Seconds between hazard spawns at match time t.
float spawnInterval(float t);
Hazard spawnHazard(std::mt19937& rng, uint32_t id);
// Last out places 1st; players out on the same tick share a place. Sorted by place.
std::vector<Placement> placements(const World& w);

}  // namespace dodge

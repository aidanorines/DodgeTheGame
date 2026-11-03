#include "shared/sim.h"

#include <algorithm>
#include <cmath>

#include "shared/constants.h"

namespace dodge {

static Input clampInput(Input in) {
  auto axis = [](float v) { return std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; };
  in.dx = axis(in.dx);
  in.dy = axis(in.dy);
  const float len = std::hypot(in.dx, in.dy);
  if (len > 1.0f) {
    in.dx /= len;
    in.dy /= len;
  }
  return in;
}

static bool insideArena(Vec2 p) { return p.x >= 0 && p.x <= kArenaW && p.y >= 0 && p.y <= kArenaH; }

static void eliminate(World& w, PlayerState& p) {
  p.alive = false;
  p.survived = w.time;
  p.outTick = w.tick;
}

World makeWorld(const std::vector<uint8_t>& playerIds, uint32_t seed) {
  World w;
  w.rng.seed(seed);
  constexpr float kHalfGap = 30.0f;  // 2x2 grid, 60 units apart
  for (size_t i = 0; i < playerIds.size(); ++i) {
    PlayerState p;
    p.id = playerIds[i];
    p.pos = {kArenaW / 2 + (i % 2 ? kHalfGap : -kHalfGap), kArenaH / 2 + (i / 2 ? kHalfGap : -kHalfGap)};
    w.players.push_back(p);
  }
  return w;
}

float spawnInterval(float t) { return std::max(0.15f, 1.0f - 0.01f * t); }

Hazard spawnHazard(std::mt19937& rng, uint32_t id) {
  const int edge = std::uniform_int_distribution<int>(0, 3)(rng);
  const float along = std::uniform_real_distribution<float>(0, 1)(rng);
  const float r = kHazardRadius, s = kHazardSpeed;
  switch (edge) {
    case 0: return {id, {-r, along * kArenaH}, {s, 0}};
    case 1: return {id, {kArenaW + r, along * kArenaH}, {-s, 0}};
    case 2: return {id, {along * kArenaW, -r}, {0, s}};
    default: return {id, {along * kArenaW, kArenaH + r}, {0, -s}};
  }
}

static bool offScreen(Vec2 p) {
  return p.x < -kHazardRadius || p.x > kArenaW + kHazardRadius || p.y < -kHazardRadius ||
         p.y > kArenaH + kHazardRadius;
}

static bool hit(const PlayerState& p, const World& w) {
  constexpr float kReach = kPlayerRadius + kHazardRadius;
  return std::any_of(w.hazards.begin(), w.hazards.end(), [&](const Hazard& h) {
    return std::hypot(h.pos.x - p.pos.x, h.pos.y - p.pos.y) < kReach;
  });
}

void stepWorld(World& w, const std::map<uint8_t, Input>& inputs, float dt) {
  ++w.tick;
  w.time += dt;

  w.spawnTimer -= dt;
  if (w.spawnTimer <= 0) {
    w.hazards.push_back(spawnHazard(w.rng, w.nextHazardId++));
    w.spawnTimer += spawnInterval(w.time);
  }
  for (auto& h : w.hazards) {
    h.pos.x += h.vel.x * dt;
    h.pos.y += h.vel.y * dt;
  }
  std::erase_if(w.hazards, [](const Hazard& h) { return offScreen(h.pos); });

  for (auto& p : w.players) {
    if (!p.alive) continue;
    if (auto it = inputs.find(p.id); it != inputs.end()) {
      const Input in = clampInput(it->second);
      p.pos.x += in.dx * kPlayerSpeed * dt;
      p.pos.y += in.dy * kPlayerSpeed * dt;
    }
    p.survived = w.time;
    if (!insideArena(p.pos) || hit(p, w)) eliminate(w, p);
  }
}

void eliminatePlayer(World& w, uint8_t id) {
  for (auto& p : w.players)
    if (p.id == id && p.alive) eliminate(w, p);
}

std::vector<Placement> placements(const World& w) {
  auto outKey = [](const PlayerState& p) { return p.alive ? UINT32_MAX : p.outTick; };
  std::vector<Placement> out;
  for (const auto& p : w.players) {
    const auto later = std::count_if(w.players.begin(), w.players.end(),
                                     [&](const PlayerState& q) { return outKey(q) > outKey(p); });
    out.push_back({p.id, static_cast<uint8_t>(1 + later), p.survived});
  }
  std::stable_sort(out.begin(), out.end(), [](const Placement& a, const Placement& b) { return a.place < b.place; });
  return out;
}

bool allEliminated(const World& w) {
  return std::none_of(w.players.begin(), w.players.end(), [](const PlayerState& p) { return p.alive; });
}

}  // namespace dodge

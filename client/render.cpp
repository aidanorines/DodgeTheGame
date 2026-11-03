#include "client/render.h"

#include <algorithm>

#include "shared/constants.h"

namespace dodge {
namespace {

constexpr Color kArenaColor = {24, 26, 32, 255};
constexpr Color kHazardColor = {230, 57, 70, 255};

// Maps arena units to window pixels, keeping 16:9 with bars on the spare sides.
struct View {
  float scale, x, y;
  Vector2 at(Vec2 p) const { return {x + p.x * scale, y + p.y * scale}; }
};

View arenaView() {
  const float w = static_cast<float>(GetScreenWidth()), h = static_cast<float>(GetScreenHeight());
  const float scale = std::min(w / kArenaW, h / kArenaH);
  return {scale, (w - kArenaW * scale) / 2, (h - kArenaH * scale) / 2};
}

Vec2 blend(Vec2 a, Vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

const LobbyPlayer* rosterEntry(const std::vector<LobbyPlayer>& roster, uint8_t id) {
  for (const auto& p : roster)
    if (p.id == id) return &p;
  return nullptr;
}

void centerText(const char* text, int y, int size, Color color) {
  DrawText(text, (GetScreenWidth() - MeasureText(text, size)) / 2, y, size, color);
}

}  // namespace

Color playerColor(uint8_t index) {
  constexpr Color kColors[4] = {SKYBLUE, ORANGE, LIME, PINK};
  return kColors[index % 4];
}

void drawArena(const Snapshot& prev, const Snapshot& cur, float alpha, const std::vector<LobbyPlayer>& roster,
               uint8_t myId) {
  const View v = arenaView();
  DrawRectangleV({v.x, v.y}, {kArenaW * v.scale, kArenaH * v.scale}, kArenaColor);
  BeginScissorMode(static_cast<int>(v.x), static_cast<int>(v.y), static_cast<int>(kArenaW * v.scale),
                   static_cast<int>(kArenaH * v.scale));

  for (const auto& h : cur.hazards) {
    Vec2 pos = h.pos;
    auto old = std::find_if(prev.hazards.begin(), prev.hazards.end(), [&](const Hazard& o) { return o.id == h.id; });
    if (old != prev.hazards.end()) pos = blend(old->pos, h.pos, alpha);
    DrawCircleV(v.at(pos), kHazardRadius * v.scale, kHazardColor);
  }

  for (const auto& p : cur.players) {
    if (!p.alive) continue;
    Vec2 pos = p.pos;
    auto old = std::find_if(prev.players.begin(), prev.players.end(), [&](const PlayerState& o) { return o.id == p.id; });
    if (old != prev.players.end()) pos = blend(old->pos, p.pos, alpha);
    const LobbyPlayer* entry = rosterEntry(roster, p.id);
    if (p.id == myId) DrawCircleV(v.at(pos), (kPlayerRadius + 3) * v.scale, RAYWHITE);
    DrawCircleV(v.at(pos), kPlayerRadius * v.scale, playerColor(entry ? entry->color : 0));
  }
  EndScissorMode();
}

void drawBigCountdown(float seconds) {
  centerText(TextFormat("%d", static_cast<int>(seconds) + 1), GetScreenHeight() / 2 - 60, 120, RAYWHITE);
}

void drawResults(const std::vector<Placement>& results, const std::vector<LobbyPlayer>& roster, float countdown) {
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
  int y = GetScreenHeight() / 2 - 170;
  centerText("Results", y, 56, RAYWHITE);
  y += 90;
  for (const auto& r : results) {
    const LobbyPlayer* entry = rosterEntry(roster, r.id);
    const char* name = entry && !entry->name.empty() ? entry->name.c_str() : "You";
    centerText(TextFormat("%d.  %s   %.1f s", r.place, name, r.survived), y, 32,
               entry ? playerColor(entry->color) : RAYWHITE);
    y += 46;
  }
  centerText(TextFormat("Back to menu in %d", static_cast<int>(std::max(countdown, 0.0f)) + 1),
             GetScreenHeight() / 2 + 170, 24, LIGHTGRAY);
}

}  // namespace dodge

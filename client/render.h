#pragma once

#include <cstdint>
#include <vector>

#include "raylib.h"
#include "shared/protocol.h"

namespace dodge {

Color playerColor(uint8_t index);

// Draws the letterboxed arena, blending prev to cur by alpha (0..1). myId gets a white ring.
void drawArena(const Snapshot& prev, const Snapshot& cur, float alpha, const std::vector<LobbyPlayer>& roster,
               uint8_t myId);
void drawBigCountdown(float seconds);
void drawResults(const std::vector<Placement>& results, const std::vector<LobbyPlayer>& roster, float countdown);

}  // namespace dodge

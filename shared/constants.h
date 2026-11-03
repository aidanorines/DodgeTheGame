#pragma once

#include <cstdint>

namespace dodge {

constexpr float kArenaW = 1280.0f;
constexpr float kArenaH = 720.0f;
constexpr int kMaxPlayers = 4;

constexpr float kLobbyCountdown = 3.0f;
constexpr float kResultsCountdown = 5.0f;
constexpr float kSoloCountdown = 3.0f;

constexpr int kMaxNameLen = 12;
constexpr int kCodeLen = 5;

constexpr uint16_t kProtocolVersion = 1;
constexpr uint16_t kGamePort = 7777;
constexpr uint16_t kLanPort = 7778;

constexpr int kTickHz = 60;
constexpr int kSnapshotHz = 30;

// Starting values; tune by playtest.
constexpr float kPlayerRadius = 16.0f;
constexpr float kPlayerSpeed = 300.0f;
constexpr float kHazardRadius = 12.0f;
constexpr float kHazardSpeed = 250.0f;

}  // namespace dodge

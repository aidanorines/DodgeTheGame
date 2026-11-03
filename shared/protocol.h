#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "shared/constants.h"
#include "shared/sim.h"

namespace dodge {

enum class Mode : uint8_t { Solo, Lan, Online };
enum class RejectReason : uint8_t { Version, BadName, Full, Started, NoSuchParty };

struct LobbyPlayer {
  uint8_t id = 0;
  uint8_t color = 0;
  std::string name;
  bool ready = false;
  bool operator==(const LobbyPlayer&) const = default;
};

// Client -> server.
struct Hello {
  uint16_t version = kProtocolVersion;
  Mode mode = Mode::Solo;
  std::string name;
  bool operator==(const Hello&) const = default;
};
struct CreateParty {
  bool operator==(const CreateParty&) const = default;
};
struct JoinParty {
  std::string code;  // empty for LAN and solo
  bool operator==(const JoinParty&) const = default;
};
struct SetReady {
  bool ready = false;
  bool operator==(const SetReady&) const = default;
};
struct InputMsg {
  Input input;
  bool operator==(const InputMsg&) const = default;
};
struct Leave {
  bool operator==(const Leave&) const = default;
};

// Server -> client.
struct Welcome {
  uint8_t yourId = 0;
  bool operator==(const Welcome&) const = default;
};
struct Reject {
  RejectReason reason = RejectReason::Version;
  bool operator==(const Reject&) const = default;
};
struct LobbyState {
  std::string code;
  std::vector<LobbyPlayer> players;
  float countdown = -1;  // < 0 when not running
  bool operator==(const LobbyState&) const = default;
};
struct MatchStart {
  float countdown = 0;
  bool operator==(const MatchStart&) const = default;
};
struct Snapshot {
  uint32_t tick = 0;
  std::vector<PlayerState> players;
  std::vector<Hazard> hazards;
  bool operator==(const Snapshot&) const = default;
};
struct MatchEnd {
  std::vector<Placement> results;
  float countdown = 0;
  bool operator==(const MatchEnd&) const = default;
};
struct PartyClosed {
  bool operator==(const PartyClosed&) const = default;
};

// The variant index is the wire tag: append new types at the end and bump kProtocolVersion.
using Message = std::variant<Hello, CreateParty, JoinParty, SetReady, InputMsg, Leave, Welcome, Reject,
                             LobbyState, MatchStart, Snapshot, MatchEnd, PartyClosed>;

std::vector<uint8_t> encode(const Message& m);
// nullopt on unknown tag, short or padded buffer, bad enum/bool, string > 64 bytes or list > 512 items.
std::optional<Message> decode(std::span<const uint8_t> bytes);

}  // namespace dodge

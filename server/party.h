#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "shared/protocol.h"
#include "shared/sim.h"

namespace dodge {

using PeerId = uint32_t;

enum class Phase { Lobby, Countdown, Match, Results, Closed };
enum class JoinResult { Ok, Full, Started, BadName };

// One party's lobby, match and results. No networking: the server feeds it events and ticks.
class Party {
 public:
  Party(Mode mode, uint32_t seed);

  JoinResult join(PeerId peer, std::string_view rawName);  // solo ignores the name
  void leave(PeerId peer);                                 // also used for disconnects
  void setReady(PeerId peer, bool ready);
  void setInput(PeerId peer, Input in);
  void tick(float dt);

  Phase phase() const { return phase_; }
  float countdown() const;  // seconds left in Countdown or Results, else -1
  std::vector<LobbyPlayer> lobbyPlayers() const;
  const World* world() const;  // non-null in Match and Results
  std::vector<Placement> results() const;
  std::optional<uint8_t> playerId(PeerId peer) const;
  bool empty() const;

 private:
  struct Member {
    PeerId peer;
    uint8_t id;
    uint8_t color;
    std::string name;
    bool ready = false;
    bool connected = true;
    Input input;
  };

  Member* find(PeerId peer);
  const Member* find(PeerId peer) const;
  int connectedCount() const;
  void updateCountdown();
  void startMatch();

  Mode mode_;
  uint32_t seed_;
  Phase phase_ = Phase::Lobby;
  float timer_ = -1;
  uint8_t nextId_ = 1;
  std::optional<PeerId> creator_;
  std::vector<Member> members_;
  std::optional<World> world_;
};

}  // namespace dodge

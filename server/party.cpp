#include "server/party.h"

#include <algorithm>
#include <map>

#include "shared/constants.h"
#include "shared/names.h"

namespace dodge {

Party::Party(Mode mode, uint32_t seed) : mode_(mode), seed_(seed) {}

Party::Member* Party::find(PeerId peer) {
  for (auto& m : members_)
    if (m.peer == peer && m.connected) return &m;
  return nullptr;
}

const Party::Member* Party::find(PeerId peer) const { return const_cast<Party*>(this)->find(peer); }

int Party::connectedCount() const {
  return static_cast<int>(std::count_if(members_.begin(), members_.end(), [](const Member& m) { return m.connected; }));
}

JoinResult Party::join(PeerId peer, std::string_view rawName) {
  if (phase_ != Phase::Lobby && phase_ != Phase::Countdown) return JoinResult::Started;
  const int cap = mode_ == Mode::Solo ? 1 : kMaxPlayers;
  if (connectedCount() >= cap || find(peer)) return JoinResult::Full;

  std::string name;
  if (mode_ != Mode::Solo) {
    auto clean = sanitizeName(rawName);
    if (!clean) return JoinResult::BadName;
    name = *clean;
  }

  uint8_t color = 0;
  while (std::any_of(members_.begin(), members_.end(), [&](const Member& m) { return m.color == color; })) ++color;

  members_.push_back({peer, nextId_++, color, std::move(name)});
  if (!creator_) creator_ = peer;

  if (mode_ == Mode::Solo) {
    phase_ = Phase::Countdown;
    timer_ = kSoloCountdown;
  } else {
    updateCountdown();  // the newcomer is not ready, so this cancels any countdown
  }
  return JoinResult::Ok;
}

void Party::leave(PeerId peer) {
  Member* m = find(peer);
  if (!m || phase_ == Phase::Closed) return;

  if (phase_ == Phase::Lobby || phase_ == Phase::Countdown) {
    std::erase_if(members_, [&](const Member& x) { return x.peer == peer; });
  } else {
    m->connected = false;  // keep the row so results can still name them
    if (world_) eliminatePlayer(*world_, m->id);
  }

  if ((mode_ == Mode::Lan && peer == creator_) || connectedCount() == 0) {
    phase_ = Phase::Closed;
    return;
  }
  if (mode_ != Mode::Solo) updateCountdown();
}

void Party::setReady(PeerId peer, bool ready) {
  if (mode_ == Mode::Solo || (phase_ != Phase::Lobby && phase_ != Phase::Countdown)) return;
  if (Member* m = find(peer)) {
    m->ready = ready;
    updateCountdown();
  }
}

void Party::setInput(PeerId peer, Input in) {
  if (Member* m = find(peer)) m->input = in;
}

void Party::updateCountdown() {
  if (phase_ != Phase::Lobby && phase_ != Phase::Countdown) return;
  const bool allReady =
      std::all_of(members_.begin(), members_.end(), [](const Member& m) { return m.ready; });
  if (members_.size() >= 2 && allReady) {
    if (phase_ == Phase::Lobby) {
      phase_ = Phase::Countdown;
      timer_ = kLobbyCountdown;
    }
  } else {
    phase_ = Phase::Lobby;
    timer_ = -1;
  }
}

void Party::startMatch() {
  std::vector<uint8_t> ids;
  for (const auto& m : members_) ids.push_back(m.id);
  world_ = makeWorld(ids, seed_);
  phase_ = Phase::Match;
  timer_ = -1;
}

void Party::tick(float dt) {
  switch (phase_) {
    case Phase::Countdown:
      timer_ -= dt;
      if (timer_ <= 0) startMatch();
      break;
    case Phase::Match: {
      std::map<uint8_t, Input> inputs;
      for (const auto& m : members_)
        if (m.connected) inputs[m.id] = m.input;
      stepWorld(*world_, inputs, dt);
      if (allEliminated(*world_)) {
        phase_ = Phase::Results;
        timer_ = kResultsCountdown;
      }
      break;
    }
    case Phase::Results:
      timer_ -= dt;
      if (timer_ <= 0) phase_ = Phase::Closed;
      break;
    default:
      break;
  }
}

float Party::countdown() const {
  return phase_ == Phase::Countdown || phase_ == Phase::Results ? timer_ : -1;
}

std::vector<LobbyPlayer> Party::lobbyPlayers() const {
  std::vector<LobbyPlayer> out;
  for (const auto& m : members_)
    if (m.connected) out.push_back({m.id, m.color, m.name, m.ready});
  return out;
}

const World* Party::world() const {
  return (phase_ == Phase::Match || phase_ == Phase::Results) && world_ ? &*world_ : nullptr;
}

std::vector<Placement> Party::results() const { return world_ ? placements(*world_) : std::vector<Placement>{}; }

std::optional<uint8_t> Party::playerId(PeerId peer) const {
  if (const Member* m = find(peer)) return m->id;
  return std::nullopt;
}

bool Party::empty() const { return connectedCount() == 0; }

}  // namespace dodge

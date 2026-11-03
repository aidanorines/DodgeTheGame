#pragma once

#include <map>
#include <random>
#include <string>
#include <string_view>

#include "server/party.h"

namespace dodge {

// Online parties by code. Lives only in server memory; closed parties free their code.
class PartyRegistry {
 public:
  explicit PartyRegistry(uint32_t seed);

  std::string create();  // new online party, returns its code
  Party* find(std::string_view rawCode);
  void tick(float dt);  // erases parties closed since the last tick, then ticks the rest
  size_t size() const { return parties_.size(); }

 private:
  std::mt19937 rng_;
  std::map<std::string, Party> parties_;  // map nodes are stable, so find() pointers survive inserts
};

}  // namespace dodge

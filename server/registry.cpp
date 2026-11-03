#include "server/registry.h"

#include "shared/party_code.h"

namespace dodge {

PartyRegistry::PartyRegistry(uint32_t seed) : rng_(seed) {}

std::string PartyRegistry::create() {
  auto code = generatePartyCode(rng_, [&](const std::string& c) { return parties_.contains(c); });
  parties_.emplace(code, Party(Mode::Online, rng_()));
  return code;
}

Party* PartyRegistry::find(std::string_view rawCode) {
  auto it = parties_.find(normalizePartyCode(rawCode));
  return it == parties_.end() ? nullptr : &it->second;
}

void PartyRegistry::tick(float dt) {
  for (auto& [code, party] : parties_) party.tick(dt);
  std::erase_if(parties_, [](const auto& kv) { return kv.second.phase() == Phase::Closed; });
}

}  // namespace dodge

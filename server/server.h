#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "shared/constants.h"
#include "shared/protocol.h"

namespace dodge {

struct ServerConfig {
  Mode mode = Mode::Online;
  uint16_t port = kGamePort;
  std::string lanName;  // shown in LAN game lists
};

// Blocks until `stop` is set. Solo and LAN also return once their single party closes.
void runServer(const ServerConfig& cfg, std::atomic<bool>& stop);

}  // namespace dodge

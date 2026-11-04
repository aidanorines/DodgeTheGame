#pragma once

#include <cstdint>
#include <string>

#include "shared/constants.h"

namespace dodge {

struct ClientConfig {
  std::string onlineHost = "127.0.0.1";
  uint16_t onlinePort = kGamePort;
};

// Reads host= and port= lines. Missing file or bad lines keep the defaults.
ClientConfig loadConfig(const std::string& path);

}  // namespace dodge

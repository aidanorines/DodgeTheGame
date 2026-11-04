#include "client/config.h"

#include <charconv>
#include <fstream>

namespace dodge {

ClientConfig loadConfig(const std::string& path) {
  ClientConfig cfg;
  std::ifstream file(path);
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.starts_with("host=") && line.size() > 5) {
      cfg.onlineHost = line.substr(5);
    } else if (line.starts_with("port=")) {
      uint16_t port = 0;
      const char* end = line.data() + line.size();
      auto [ptr, ec] = std::from_chars(line.data() + 5, end, port);
      if (ec == std::errc{} && ptr == end && port != 0) cfg.onlinePort = port;
    }
  }
  return cfg;
}

}  // namespace dodge

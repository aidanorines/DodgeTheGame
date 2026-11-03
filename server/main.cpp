#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "server/server.h"

static std::atomic<bool> gStop{false};

static void onSignal(int) { gStop = true; }

int main(int argc, char** argv) {
  dodge::ServerConfig cfg;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      const int port = std::atoi(argv[++i]);
      if (port < 1 || port > 65535) {
        std::fprintf(stderr, "dodge_server: bad port\n");
        return 1;
      }
      cfg.port = static_cast<uint16_t>(port);
    } else {
      std::fprintf(stderr, "usage: dodge_server [--port N]\n");
      return 1;
    }
  }
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
  std::printf("dodge_server listening on UDP %u\n", static_cast<unsigned>(cfg.port));
  std::fflush(stdout);
  dodge::runServer(cfg, gStop);
  return 0;
}

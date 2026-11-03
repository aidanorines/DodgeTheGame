#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "shared/constants.h"

namespace dodge {

struct LanGame {
  std::string hostName;
  uint32_t ip = 0;  // network byte order, as ENet stores it
  uint16_t port = 0;
  uint8_t players = 0;
};

std::vector<uint8_t> encodeAnnounce(const LanGame& g);
// Untrusted input from the LAN. `ip` is the sender address. nullopt if malformed or another version.
std::optional<LanGame> parseAnnounce(std::span<const uint8_t> bytes, uint32_t ip);

class LanAnnouncer {
 public:
  // target defaults to the broadcast address. Needs enet_initialize.
  explicit LanAnnouncer(uint16_t discoveryPort = kLanPort, uint32_t target = 0xFFFFFFFFu);
  ~LanAnnouncer();
  LanAnnouncer(const LanAnnouncer&) = delete;
  LanAnnouncer& operator=(const LanAnnouncer&) = delete;

  void announce(const LanGame& g);  // call about once a second

 private:
  uintptr_t socket_;
  uint16_t port_;
  uint32_t target_;
};

class LanBrowser {
 public:
  explicit LanBrowser(uint16_t discoveryPort = kLanPort);  // needs enet_initialize
  ~LanBrowser();
  LanBrowser(const LanBrowser&) = delete;
  LanBrowser& operator=(const LanBrowser&) = delete;

  void poll(double now);  // reads pending announcements; drops games unseen for 3 s
  std::vector<LanGame> games() const;

 private:
  struct Entry {
    LanGame game;
    double lastSeen;
  };
  uintptr_t socket_;
  std::vector<Entry> entries_;
};

}  // namespace dodge

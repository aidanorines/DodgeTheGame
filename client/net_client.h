#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shared/protocol.h"

struct _ENetHost;
struct _ENetPeer;

namespace dodge {

// ENet stays out of this header: its Windows headers clash with raylib.h.
class NetClient {
 public:
  NetClient();  // also initializes ENet, so create it before any LanBrowser
  ~NetClient();
  NetClient(const NetClient&) = delete;
  NetClient& operator=(const NetClient&) = delete;

  bool connect(const std::string& host, uint16_t port);  // async: messages queue until connected
  bool connectIp(uint32_t ip, uint16_t port);            // ip in network byte order
  void send(const Message& m);
  std::vector<Message> poll();
  void disconnect();

  bool active() const { return peer_ != nullptr; }
  bool lost() const { return lost_; }  // the connection failed or dropped since connect

 private:
  bool open(uint32_t ip, uint16_t port);
  void transmit(const Message& m);

  _ENetHost* host_ = nullptr;
  _ENetPeer* peer_ = nullptr;
  bool connected_ = false;
  bool lost_ = false;
  std::vector<Message> pending_;
};

}  // namespace dodge

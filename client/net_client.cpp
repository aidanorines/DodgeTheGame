#include "client/net_client.h"

#include <enet/enet.h>

namespace dodge {

NetClient::NetClient() { enet_initialize(); }

NetClient::~NetClient() {
  disconnect();
  enet_deinitialize();
}

bool NetClient::connect(const std::string& host, uint16_t port) {
  ENetAddress addr{};
  if (enet_address_set_host(&addr, host.c_str()) != 0) {
    lost_ = true;
    return false;
  }
  return open(addr.host, port);
}

bool NetClient::connectIp(uint32_t ip, uint16_t port) { return open(ip, port); }

bool NetClient::open(uint32_t ip, uint16_t port) {
  disconnect();
  lost_ = false;
  host_ = enet_host_create(nullptr, 1, 2, 0, 0);
  if (!host_) {
    lost_ = true;
    return false;
  }
  ENetAddress addr{};
  addr.host = ip;
  addr.port = port;
  peer_ = enet_host_connect(host_, &addr, 2, 0);
  if (!peer_) {
    disconnect();
    lost_ = true;
    return false;
  }
  enet_peer_timeout(peer_, 0, 2000, 5000);  // notice a dead server in 5 s, not ENet's 30 s
  return true;
}

void NetClient::transmit(const Message& m) {
  const bool fast = std::holds_alternative<InputMsg>(m);
  const auto bytes = encode(m);
  ENetPacket* packet =
      enet_packet_create(bytes.data(), bytes.size(), fast ? ENET_PACKET_FLAG_UNSEQUENCED : ENET_PACKET_FLAG_RELIABLE);
  if (enet_peer_send(peer_, fast ? 1 : 0, packet) < 0) enet_packet_destroy(packet);
}

void NetClient::send(const Message& m) {
  if (!peer_) return;
  if (connected_)
    transmit(m);
  else
    pending_.push_back(m);
}

std::vector<Message> NetClient::poll() {
  std::vector<Message> out;
  if (!host_) return out;
  ENetEvent e;
  while (host_ && enet_host_service(host_, &e, 0) > 0) {
    switch (e.type) {
      case ENET_EVENT_TYPE_CONNECT:
        connected_ = true;
        for (const auto& m : pending_) transmit(m);
        pending_.clear();
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        if (auto m = decode({e.packet->data, e.packet->dataLength})) out.push_back(*m);
        enet_packet_destroy(e.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        lost_ = true;
        connected_ = false;
        peer_ = nullptr;
        break;
      default:
        break;
    }
  }
  return out;
}

void NetClient::disconnect() {
  if (peer_) {
    enet_peer_disconnect_now(peer_, 0);
    peer_ = nullptr;
  }
  if (host_) {
    enet_host_flush(host_);
    enet_host_destroy(host_);
    host_ = nullptr;
  }
  connected_ = false;
  lost_ = false;
  pending_.clear();
}

}  // namespace dodge

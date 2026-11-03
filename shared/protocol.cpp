#include "shared/protocol.h"

#include <bit>
#include <utility>

namespace dodge {
namespace {

constexpr size_t kMaxString = 64;
constexpr size_t kMaxList = 512;

struct Writer {
  std::vector<uint8_t> out;
};

struct Reader {
  std::span<const uint8_t> in;
  size_t pos = 0;
  bool ok = true;
  bool need(size_t n) {
    if (in.size() - pos < n) ok = false;
    return ok;
  }
};

// Primitives, little-endian. Each type has a Writer and a Reader overload.
void io(Writer& w, uint8_t& v) { w.out.push_back(v); }
void io(Reader& r, uint8_t& v) {
  if (r.need(1)) v = r.in[r.pos++];
}

void io(Writer& w, uint16_t& v) {
  w.out.push_back(static_cast<uint8_t>(v));
  w.out.push_back(static_cast<uint8_t>(v >> 8));
}
void io(Reader& r, uint16_t& v) {
  if (!r.need(2)) return;
  v = static_cast<uint16_t>(r.in[r.pos] | r.in[r.pos + 1] << 8);
  r.pos += 2;
}

void io(Writer& w, uint32_t& v) {
  for (int i = 0; i < 4; ++i) w.out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void io(Reader& r, uint32_t& v) {
  if (!r.need(4)) return;
  v = 0;
  for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(r.in[r.pos + i]) << (8 * i);
  r.pos += 4;
}

void io(Writer& w, float& v) {
  auto bits = std::bit_cast<uint32_t>(v);
  io(w, bits);
}
void io(Reader& r, float& v) {
  uint32_t bits = 0;
  io(r, bits);
  v = std::bit_cast<float>(bits);
}

void io(Writer& w, bool& v) { w.out.push_back(v ? 1 : 0); }
void io(Reader& r, bool& v) {
  uint8_t b = 0;
  io(r, b);
  if (b > 1) r.ok = false;
  v = b == 1;
}

template <class E, E kLast>
void ioEnum(Writer& w, E& v) {
  auto b = static_cast<uint8_t>(v);
  io(w, b);
}
template <class E, E kLast>
void ioEnum(Reader& r, E& v) {
  uint8_t b = 0;
  io(r, b);
  if (b > static_cast<uint8_t>(kLast)) r.ok = false;
  v = static_cast<E>(b);
}
template <class S>
void io(S& s, Mode& v) { ioEnum<Mode, Mode::Online>(s, v); }
template <class S>
void io(S& s, RejectReason& v) { ioEnum<RejectReason, RejectReason::NoSuchParty>(s, v); }

void io(Writer& w, std::string& v) {
  auto len = static_cast<uint8_t>(v.size());
  io(w, len);
  w.out.insert(w.out.end(), v.begin(), v.begin() + len);
}
void io(Reader& r, std::string& v) {
  uint8_t len = 0;
  io(r, len);
  if (len > kMaxString) r.ok = false;
  if (!r.need(len)) return;
  v.assign(reinterpret_cast<const char*>(r.in.data() + r.pos), len);
  r.pos += len;
}

template <class T>
void io(Writer& w, std::vector<T>& v) {
  auto n = static_cast<uint16_t>(v.size());
  io(w, n);
  for (auto& e : v) io(w, e);
}
template <class T>
void io(Reader& r, std::vector<T>& v) {
  uint16_t n = 0;
  io(r, n);
  if (n > kMaxList) r.ok = false;
  if (!r.ok) return;
  v.resize(n);
  for (auto& e : v) io(r, e);
}

// Composites: one field list shared by encode and decode.
template <class S> void io(S& s, Vec2& v) { io(s, v.x); io(s, v.y); }
template <class S> void io(S& s, Input& v) { io(s, v.dx); io(s, v.dy); }
template <class S> void io(S& s, PlayerState& v) { io(s, v.id); io(s, v.pos); io(s, v.alive); io(s, v.survived); io(s, v.outTick); }
template <class S> void io(S& s, Hazard& v) { io(s, v.id); io(s, v.pos); io(s, v.vel); }
template <class S> void io(S& s, Placement& v) { io(s, v.id); io(s, v.place); io(s, v.survived); }
template <class S> void io(S& s, LobbyPlayer& v) { io(s, v.id); io(s, v.color); io(s, v.name); io(s, v.ready); }

template <class S> void io(S& s, Hello& m) { io(s, m.version); io(s, m.mode); io(s, m.name); }
template <class S> void io(S&, CreateParty&) {}
template <class S> void io(S& s, JoinParty& m) { io(s, m.code); }
template <class S> void io(S& s, SetReady& m) { io(s, m.ready); }
template <class S> void io(S& s, InputMsg& m) { io(s, m.input); }
template <class S> void io(S&, Leave&) {}
template <class S> void io(S& s, Welcome& m) { io(s, m.yourId); }
template <class S> void io(S& s, Reject& m) { io(s, m.reason); }
template <class S> void io(S& s, LobbyState& m) { io(s, m.code); io(s, m.players); io(s, m.countdown); }
template <class S> void io(S& s, MatchStart& m) { io(s, m.countdown); }
template <class S> void io(S& s, Snapshot& m) { io(s, m.tick); io(s, m.players); io(s, m.hazards); }
template <class S> void io(S& s, MatchEnd& m) { io(s, m.results); io(s, m.countdown); }
template <class S> void io(S&, PartyClosed&) {}

template <size_t... I>
std::optional<Message> decodeTag(size_t tag, Reader& r, std::index_sequence<I...>) {
  std::optional<Message> out;
  ((tag == I ? (out.emplace(std::in_place_index<I>), io(r, std::get<I>(*out)), 0) : 0), ...);
  return out;
}

}  // namespace

std::vector<uint8_t> encode(const Message& m) {
  Writer w;
  auto tag = static_cast<uint8_t>(m.index());
  io(w, tag);
  Message copy = m;  // io takes non-const refs so one field list serves both directions
  std::visit([&](auto& msg) { io(w, msg); }, copy);
  return std::move(w.out);
}

std::optional<Message> decode(std::span<const uint8_t> bytes) {
  Reader r{bytes};
  uint8_t tag = 0;
  io(r, tag);
  if (!r.ok || tag >= std::variant_size_v<Message>) return std::nullopt;
  auto out = decodeTag(tag, r, std::make_index_sequence<std::variant_size_v<Message>>{});
  if (!r.ok || r.pos != bytes.size()) return std::nullopt;
  return out;
}

}  // namespace dodge

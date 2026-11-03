#include <algorithm>
#include <atomic>
#include <optional>
#include <string>
#include <thread>

#include "client/net_client.h"
#include "client/render.h"
#include "client/screens.h"
#include "raylib.h"
#include "server/lan.h"
#include "server/server.h"
#include "shared/constants.h"

using namespace dodge;

namespace {

// shortcut: fixed address, Task 12 reads it from dodge.cfg.
constexpr const char* kOnlineHost = "127.0.0.1";
// shortcut: fixed port, so two solo games on one PC clash; pick a free port if that ever matters.
constexpr uint16_t kSoloPort = kGamePort + 2;

const char* rejectText(RejectReason r) {
  switch (r) {
    case RejectReason::Version: return "Game version mismatch";
    case RejectReason::BadName: return "That name is not allowed";
    case RejectReason::Full: return "Party full";
    case RejectReason::Started: return "Match already started";
    default: return "No party with that code";
  }
}

class App {
 public:
  void frame() {
    pumpNetwork();
    tickCountdown();
    pollLan();

    BeginDrawing();
    s_.action = Action::None;
    Screen next = s_.screen;
    if (inArena()) {
      ClearBackground(BLACK);
      drawArena(prev_, cur_, blendAlpha(), s_.lobby.players, s_.myId);
      if (s_.screen == Screen::Countdown) drawBigCountdown(std::max(s_.countdown, 0.0f));
      if (s_.screen == Screen::Match) drawMatchOverlay(s_, alive());
      if (s_.screen == Screen::Results) drawResults(results_, s_.lobby.players, s_.countdown);
    } else {
      ClearBackground(RAYWHITE);
      next = drawMenu(s_);
    }
    EndDrawing();

    s_.screen = next;
    act();
    if (s_.screen == Screen::Match && alive()) sendInput();
  }

  void shutdown() { endSession(); }

 private:
  bool inArena() const {
    return s_.screen == Screen::Countdown || s_.screen == Screen::Match || s_.screen == Screen::Results;
  }

  bool alive() const {
    return std::any_of(cur_.players.begin(), cur_.players.end(),
                       [&](const PlayerState& p) { return p.id == s_.myId && p.alive; });
  }

  float blendAlpha() const {
    return std::clamp(static_cast<float>(GetTime() - curAt_) * kSnapshotHz, 0.0f, 1.0f);
  }

  void startLocalServer(Mode mode, uint16_t port, const std::string& name) {
    stopLocalServer();
    stop_ = false;
    server_ = std::thread([this, mode, port, name] { runServer({mode, port, name}, stop_); });
  }

  void stopLocalServer() {
    stop_ = true;
    if (server_.joinable()) server_.join();
  }

  // Back to a clean state. The caller picks the screen.
  void endSession() {
    net_.disconnect();
    stopLocalServer();
    inSession_ = false;
    s_.lobby = {};
    s_.myId = 0;
    s_.ready = false;
    s_.countdown = -1;
    prev_ = cur_ = {};
    results_.clear();
  }

  void hello(Mode mode) { net_.send(Hello{kProtocolVersion, mode, mode == Mode::Solo ? "" : s_.name}); }

  void act() {
    switch (s_.action) {
      case Action::StartSolo:
        if (net_.active()) break;
        s_.netMode = Mode::Solo;
        startLocalServer(Mode::Solo, kSoloPort, "");
        net_.connect("127.0.0.1", kSoloPort);
        hello(Mode::Solo);
        break;
      case Action::Create:
        if (net_.active()) break;
        if (s_.netMode == Mode::Lan) {
          startLocalServer(Mode::Lan, kGamePort, s_.name);
          net_.connect("127.0.0.1", kGamePort);
          hello(Mode::Lan);
        } else {
          net_.connect(kOnlineHost, kGamePort);
          hello(Mode::Online);
          net_.send(CreateParty{});
        }
        break;
      case Action::JoinLan:
        if (net_.active() || s_.lanChoice >= s_.lanGames.size()) break;
        net_.connectIp(s_.lanGames[s_.lanChoice].ip, s_.lanGames[s_.lanChoice].port);
        hello(Mode::Lan);
        break;
      case Action::JoinCode:
        if (net_.active()) break;
        net_.connect(kOnlineHost, kGamePort);
        hello(Mode::Online);
        net_.send(JoinParty{s_.code});
        break;
      case Action::ToggleReady:
        s_.ready = !s_.ready;
        net_.send(SetReady{s_.ready});
        break;
      case Action::Leave:
        endSession();
        s_.screen = Screen::Main;
        break;
      default:
        break;
    }
  }

  void sendInput() {
    Input in;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) in.dx -= 1;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) in.dx += 1;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) in.dy -= 1;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) in.dy += 1;
    net_.send(InputMsg{in});
  }

  void pumpNetwork() {
    for (const Message& m : net_.poll()) {
      if (auto* w = std::get_if<Welcome>(&m)) {
        s_.myId = w->yourId;
        inSession_ = true;
      } else if (auto* r = std::get_if<Reject>(&m)) {
        s_.error = rejectText(r->reason);
        endSession();  // stays on the menu that tried to join
        return;
      } else if (auto* l = std::get_if<LobbyState>(&m)) {
        s_.lobby = *l;
        s_.countdown = l->countdown;
        s_.screen = s_.netMode == Mode::Solo ? Screen::Countdown : Screen::Lobby;
      } else if (std::holds_alternative<MatchStart>(m)) {
        s_.countdown = -1;
        s_.screen = Screen::Match;
      } else if (auto* snap = std::get_if<Snapshot>(&m)) {
        if (snap->tick > cur_.tick) {
          prev_ = std::move(cur_);
          cur_ = *snap;
          curAt_ = GetTime();
        }
      } else if (auto* end = std::get_if<MatchEnd>(&m)) {
        results_ = end->results;
        s_.countdown = end->countdown;
        s_.screen = Screen::Results;
      } else if (std::holds_alternative<PartyClosed>(m)) {
        endSession();
        s_.screen = Screen::Main;
        return;
      }
    }
    if (net_.lost()) {
      const bool wasIn = inSession_;
      s_.error = wasIn ? "Connection lost" : "Could not reach the server";
      endSession();
      if (wasIn) s_.screen = Screen::Main;
    }
  }

  void tickCountdown() {
    if (s_.countdown > 0) s_.countdown = std::max(0.0f, s_.countdown - GetFrameTime());
  }

  void pollLan() {
    if (s_.screen != Screen::LanList) {
      browser_.reset();
      return;
    }
    if (!browser_) browser_.emplace();
    browser_->poll(GetTime());
    s_.lanGames = browser_->games();
  }

  ClientState s_;
  NetClient net_;
  std::optional<LanBrowser> browser_;
  std::thread server_;
  std::atomic<bool> stop_{false};
  bool inSession_ = false;
  Snapshot prev_, cur_;
  double curAt_ = 0;
  std::vector<Placement> results_;
};

}  // namespace

int main() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(static_cast<int>(kArenaW), static_cast<int>(kArenaH), "Dodge");
  SetTargetFPS(60);
  initMenuStyle();

  {
    App app;
    while (!WindowShouldClose()) app.frame();
    app.shutdown();
  }
  CloseWindow();
  return 0;
}

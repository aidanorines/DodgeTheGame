#include "client/screens.h"

#include <cstring>

#include "client/render.h"
#include "raylib.h"
#include "shared/names.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

namespace dodge {
namespace {

constexpr float kRowW = 360;
constexpr float kRowH = 50;
constexpr float kRowGap = 66;

Rectangle row(int i) {
  return {(GetScreenWidth() - kRowW) / 2, GetScreenHeight() / 2.0f - 110 + i * kRowGap, kRowW, kRowH};
}

bool button(int i, const char* text) { return GuiButton(row(i), text) != 0; }

void centerText(const char* text, float y, int size, Color color) {
  DrawText(text, (GetScreenWidth() - MeasureText(text, size)) / 2, static_cast<int>(y), size, color);
}

void title(const char* text) { centerText(text, GetScreenHeight() / 2.0f - 220, 56, DARKGRAY); }

void error(const ClientState& s) {
  if (!s.error.empty()) centerText(s.error.c_str(), GetScreenHeight() - 70.0f, 22, MAROON);
}

bool enabledButton(int i, const char* text, bool enabled) {
  if (!enabled) GuiDisable();
  const bool pressed = button(i, text);
  GuiEnable();
  return pressed && enabled;
}

void act(ClientState& s, Action a) {
  s.action = a;
  s.error.clear();
}

Screen drawMain(ClientState& s) {
  title("DODGE");
  error(s);
  return button(1, "Play") ? Screen::Play : Screen::Main;
}

Screen drawPlay(ClientState& s) {
  title("Play");
  if (button(0, "Solo")) act(s, Action::StartSolo);
  if (button(1, "Multiplayer")) return Screen::Name;
  if (button(3, "Back")) return Screen::Main;
  return Screen::Play;
}

Screen drawName(ClientState& s) {
  title("Your name");
  if (GuiTextBox(row(0), s.name, sizeof s.name, s.editingName)) s.editingName = !s.editingName;
  if (enabledButton(1, "Continue", sanitizeName(s.name).has_value())) return Screen::NetMode;
  if (button(3, "Back")) return Screen::Play;
  return Screen::Name;
}

Screen drawNetMode(ClientState& s) {
  title("Multiplayer");
  if (button(0, "LAN")) {
    s.netMode = Mode::Lan;
    return Screen::CreateJoin;
  }
  if (button(1, "Online")) {
    s.netMode = Mode::Online;
    return Screen::CreateJoin;
  }
  if (button(3, "Back")) return Screen::Name;
  return Screen::NetMode;
}

Screen drawCreateJoin(ClientState& s) {
  title(s.netMode == Mode::Lan ? "LAN" : "Online");
  error(s);
  if (button(0, "Create")) act(s, Action::Create);
  if (button(1, "Join")) {
    s.error.clear();
    return s.netMode == Mode::Lan ? Screen::LanList : Screen::CodeEntry;
  }
  if (button(3, "Back")) return Screen::NetMode;
  return Screen::CreateJoin;
}

Screen drawLanList(ClientState& s) {
  title("LAN games");
  error(s);
  if (s.lanGames.empty()) centerText("Searching the network...", row(0).y + 12, 22, GRAY);
  for (size_t i = 0; i < s.lanGames.size() && i < 3; ++i) {
    const auto& g = s.lanGames[i];
    if (button(static_cast<int>(i), TextFormat("%s  %d/%d", g.hostName.c_str(), g.players, kMaxPlayers))) {
      s.lanChoice = i;
      act(s, Action::JoinLan);
    }
  }
  if (button(3, "Back")) return Screen::CreateJoin;
  return Screen::LanList;
}

Screen drawCodeEntry(ClientState& s) {
  title("Party code");
  error(s);
  if (GuiTextBox(row(0), s.code, sizeof s.code, s.editingCode)) s.editingCode = !s.editingCode;
  if (enabledButton(1, "Join", std::strlen(s.code) == static_cast<size_t>(kCodeLen))) act(s, Action::JoinCode);
  if (button(3, "Back")) return Screen::CreateJoin;
  return Screen::CodeEntry;
}

Screen drawLobby(ClientState& s) {
  title("Lobby");
  if (!s.lobby.code.empty()) centerText(TextFormat("Party code: %s", s.lobby.code.c_str()), row(0).y - 46, 28, DARKGRAY);

  const float x = row(0).x;
  float y = row(0).y;
  for (const auto& p : s.lobby.players) {
    DrawCircle(static_cast<int>(x + 16), static_cast<int>(y + 16), 14, playerColor(p.color));
    DrawText(p.name.c_str(), static_cast<int>(x + 44), static_cast<int>(y + 4), 24, DARKGRAY);
    if (p.ready) DrawText("Ready", static_cast<int>(x + kRowW - 80), static_cast<int>(y + 4), 24, DARKGREEN);
    y += 40;
  }

  if (s.countdown >= 0) centerText(TextFormat("Starting in %d", static_cast<int>(s.countdown) + 1), row(3).y - 50, 28, MAROON);
  if (button(3, s.ready ? "Not ready" : "Ready")) act(s, Action::ToggleReady);
  if (button(4, "Leave")) act(s, Action::Leave);
  return Screen::Lobby;
}

}  // namespace

void drawMatchOverlay(ClientState& s, bool alive) {
  if (!alive) centerText("Spectating", 16, 28, RAYWHITE);
  if (s.netMode == Mode::Online && GuiButton({GetScreenWidth() - 136.0f, 12, 120, 40}, "Leave")) act(s, Action::Leave);
}

void initMenuStyle() { GuiSetStyle(DEFAULT, TEXT_SIZE, 24); }

Screen drawMenu(ClientState& s) {
  switch (s.screen) {
    case Screen::Main: return drawMain(s);
    case Screen::Play: return drawPlay(s);
    case Screen::Name: return drawName(s);
    case Screen::NetMode: return drawNetMode(s);
    case Screen::CreateJoin: return drawCreateJoin(s);
    case Screen::LanList: return drawLanList(s);
    case Screen::CodeEntry: return drawCodeEntry(s);
    case Screen::Lobby: return drawLobby(s);
    default: return s.screen;
  }
}

}  // namespace dodge

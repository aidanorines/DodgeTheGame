#pragma once

#include <string>
#include <vector>

#include "server/lan.h"
#include "shared/constants.h"
#include "shared/protocol.h"

namespace dodge {

enum class Screen { Main, Play, Name, NetMode, CreateJoin, LanList, CodeEntry, Lobby, Countdown, Match, Results };

// What the player asked for this frame. Screens only record it; main acts on it.
enum class Action { None, StartSolo, Create, JoinLan, JoinCode, ToggleReady, Leave };

struct ClientState {
  Screen screen = Screen::Main;
  Action action = Action::None;
  Mode netMode = Mode::Lan;
  std::string error;  // shown on menu screens until the next action

  char name[kMaxNameLen + 1] = "";
  char code[kCodeLen + 1] = "";
  bool editingName = true;
  bool editingCode = true;

  std::vector<LanGame> lanGames;
  size_t lanChoice = 0;

  LobbyState lobby;
  uint8_t myId = 0;
  bool ready = false;
  float countdown = -1;  // counted down locally between server updates
};

void initMenuStyle();  // call once after InitWindow

// Spectating label, and the Leave button online (records Action::Leave).
void drawMatchOverlay(ClientState& s, bool alive);

// Draws the current menu screen and returns the screen to show next frame.
// Match, Countdown and Results are drawn by render.h.
Screen drawMenu(ClientState& s);

}  // namespace dodge

#include "client/screens.h"
#include "raylib.h"
#include "shared/constants.h"

using namespace dodge;

int main() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(static_cast<int>(kArenaW), static_cast<int>(kArenaH), "Dodge");
  SetTargetFPS(60);
  initMenuStyle();

  ClientState s;
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    s.action = Action::None;
    Screen next = drawMenu(s);
    EndDrawing();

    // shortcut: no networking yet, Task 11 replaces this with real actions.
    if (s.action == Action::Create || s.action == Action::JoinLan || s.action == Action::JoinCode) next = Screen::Lobby;
    if (s.action == Action::Leave) next = Screen::Main;
    s.screen = next;
  }
  CloseWindow();
  return 0;
}

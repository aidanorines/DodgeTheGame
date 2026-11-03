#include "raylib.h"
#include "shared/constants.h"

int main() {
  InitWindow(static_cast<int>(dodge::kArenaW), static_cast<int>(dodge::kArenaH), "Dodge");
  SetTargetFPS(60);
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
  }
  CloseWindow();
  return 0;
}

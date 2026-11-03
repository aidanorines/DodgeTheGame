#include "client/render.h"

namespace dodge {

Color playerColor(uint8_t index) {
  constexpr Color kColors[4] = {SKYBLUE, ORANGE, LIME, PINK};
  return kColors[index % 4];
}

}  // namespace dodge

#include <doctest/doctest.h>

#include "shared/constants.h"

TEST_CASE("arena size") {
  CHECK(dodge::kArenaW == 1280);
  CHECK(dodge::kArenaH == 720);
}

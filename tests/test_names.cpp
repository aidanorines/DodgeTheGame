#include <doctest/doctest.h>

#include "shared/names.h"

using dodge::sanitizeName;

TEST_CASE("names") {
  CHECK(sanitizeName("  Ann  ") == "Ann");
  CHECK(sanitizeName("a_b-C 9") == "a_b-C 9");
  CHECK(sanitizeName("123456789012") == "123456789012");
  CHECK(!sanitizeName("1234567890123"));  // 13 chars
  CHECK(!sanitizeName("   "));
  CHECK(!sanitizeName(""));
  CHECK(!sanitizeName("bad!"));
  CHECK(!sanitizeName("caf\xC3\xA9"));  // non-ASCII
}

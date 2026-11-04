#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>

#include "client/config.h"

using namespace dodge;

TEST_CASE("config defaults when the file is missing") {
  const auto cfg = loadConfig("no-such-file.cfg");
  CHECK(cfg.onlineHost == "127.0.0.1");
  CHECK(cfg.onlinePort == kGamePort);
}

TEST_CASE("config reads host and port and skips bad values") {
  const char* path = "test_dodge.cfg";
  std::ofstream(path) << "host=play.example.com\r\nport=9000\n# comment\n";
  auto cfg = loadConfig(path);
  CHECK(cfg.onlineHost == "play.example.com");
  CHECK(cfg.onlinePort == 9000);

  std::ofstream(path) << "port=99999\nport=abc\nport=0\n";
  cfg = loadConfig(path);
  CHECK(cfg.onlinePort == kGamePort);
  std::remove(path);
}

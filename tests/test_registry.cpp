#include <doctest/doctest.h>

#include <cctype>
#include <set>

#include "server/registry.h"
#include "shared/constants.h"
#include "shared/party_code.h"

using namespace dodge;

TEST_CASE("code shape") {
  std::mt19937 rng(5);
  for (int i = 0; i < 1000; ++i) {
    const auto code = generatePartyCode(rng, [](const std::string&) { return false; });
    REQUIRE(code.size() == static_cast<size_t>(kCodeLen));
    for (char c : code) CHECK(kCodeAlphabet.find(c) != std::string_view::npos);
  }
}

TEST_CASE("collision rerolls") {
  std::mt19937 rng(5);
  std::vector<std::string> seen;
  const auto code = generatePartyCode(rng, [&](const std::string& c) {
    seen.push_back(c);
    return seen.size() <= 3;
  });
  REQUIRE(seen.size() == 4);
  CHECK(code == seen[3]);
}

TEST_CASE("normalize trims and uppercases") {
  CHECK(normalizePartyCode("  abcde ") == "ABCDE");
  CHECK(normalizePartyCode("XyZ23") == "XYZ23");
}

TEST_CASE("lookup is case-insensitive") {
  PartyRegistry reg(9);
  const auto code = reg.create();
  std::string lower = code;
  for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  CHECK(reg.find(" " + lower + " ") != nullptr);
  CHECK(reg.find("00000") == nullptr);  // '0' is not in the alphabet
}

TEST_CASE("codes are unique among live parties") {
  PartyRegistry reg(9);
  std::set<std::string> codes;
  for (int i = 0; i < 200; ++i) codes.insert(reg.create());
  CHECK(codes.size() == 200);
  CHECK(reg.size() == 200);
}

TEST_CASE("closed parties are freed") {
  PartyRegistry reg(9);
  const auto code = reg.create();
  Party* p = reg.find(code);
  REQUIRE(p != nullptr);
  p->join(1, "A");
  p->leave(1);
  REQUIRE(p->phase() == Phase::Closed);
  reg.tick(1.0f / kTickHz);
  CHECK(reg.size() == 0);
  CHECK(reg.find(code) == nullptr);
}

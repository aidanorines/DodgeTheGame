#pragma once

#include <functional>
#include <random>
#include <string>
#include <string_view>

namespace dodge {

// No look-alikes: 0 O 1 I L are left out.
inline constexpr std::string_view kCodeAlphabet = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";

// Draws kCodeLen-char codes until `taken` returns false for one.
std::string generatePartyCode(std::mt19937& rng, const std::function<bool(const std::string&)>& taken);
// Trims surrounding spaces and uppercases, so typed codes match generated ones.
std::string normalizePartyCode(std::string_view raw);

}  // namespace dodge

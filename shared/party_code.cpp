#include "shared/party_code.h"

#include <cctype>

#include "shared/constants.h"

namespace dodge {

std::string generatePartyCode(std::mt19937& rng, const std::function<bool(const std::string&)>& taken) {
  std::uniform_int_distribution<size_t> pick(0, kCodeAlphabet.size() - 1);
  std::string code(kCodeLen, ' ');
  do {
    for (char& c : code) c = kCodeAlphabet[pick(rng)];
  } while (taken(code));
  return code;
}

std::string normalizePartyCode(std::string_view raw) {
  const auto first = raw.find_first_not_of(' ');
  if (first == std::string_view::npos) return {};
  std::string out(raw.substr(first, raw.find_last_not_of(' ') - first + 1));
  for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return out;
}

}  // namespace dodge

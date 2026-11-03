#include "shared/names.h"

#include "shared/constants.h"

namespace dodge {

static bool allowed(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' ||
         c == '_' || c == '-';
}

std::optional<std::string> sanitizeName(std::string_view raw) {
  const auto first = raw.find_first_not_of(' ');
  if (first == std::string_view::npos) return std::nullopt;
  raw = raw.substr(first, raw.find_last_not_of(' ') - first + 1);
  if (raw.size() > static_cast<size_t>(kMaxNameLen)) return std::nullopt;
  for (char c : raw)
    if (!allowed(c)) return std::nullopt;
  return std::string(raw);
}

}  // namespace dodge

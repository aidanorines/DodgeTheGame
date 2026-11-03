#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace dodge {

// Trimmed name if valid (1..kMaxNameLen of letters, digits, space, '_', '-'), else nullopt.
std::optional<std::string> sanitizeName(std::string_view raw);

}  // namespace dodge

#pragma once

#include <string>

namespace t81::support {
// Quote a UTF-8 JSON string, escaping quotes, backslashes and control bytes.
// This is string encoding, not JSON canonicalization or UTF-8 validation.
inline std::string json_string(const std::string& value) {
  static constexpr char hex[] = "0123456789abcdef";
  std::string out = "\"";
  for (char byte : value) {
    const auto c = static_cast<unsigned char>(byte);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 0x20) {
      out += "\\u00";
      out += hex[c >> 4];
      out += hex[c & 15];
    } else {
      out += static_cast<char>(c);
    }
  }
  return out + "\"";
}
}  // namespace t81::support

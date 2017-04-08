#include "rapidcheck/Show.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <locale>

namespace rc {
namespace detail {

void showValue(const std::string &value, std::ostream &os) {
  const auto &locale = std::locale::classic();
  os << '"';
  for (char c : value) {
    if (!std::isspace(c, locale) && std::isprint(c, locale)) {
      switch (c) {
      case '\\':
        os << "\\";
        break;
      case '"':
        os << "\\\"";
        break;
      default:
        os << c;
      }
    } else {
      switch (c) {
      case 0x00:
        os << "\\0";
        break;
      case 0x07:
        os << "\\a";
        break;
      case 0x08:
        os << "\\b";
        break;
      case 0x0C:
        os << "\\f";
        break;
      case 0x0A:
        os << "\\n";
        break;
      case 0x0D:
        os << "\\r";
        break;
      case 0x09:
        os << "\\t";
        break;
      case 0x0B:
        os << "\\v";
        break;
      case ' ':
        os << ' ';
        break;
      default:
        auto flags = os.flags();
        os << "\\x";
        os << std::hex << std::setprecision(2) << std::uppercase
           << static_cast<int>(c & 0xFF);
        os.flags(flags);
      }
    }
  }
  os << '"';
}

void showValue(const char *value, std::ostream &os) {
  show(std::string(value), os);
}

} // namespace detail
} // namespace rc

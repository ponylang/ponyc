#include "rapidcheck/Assertions.h"

namespace rc {
namespace detail {

std::string makeMessage(const std::string &file,
                        int line,
                        const std::string &assertion,
                        const std::string &extra) {
  auto msg = file + ":" + std::to_string(line) + ":\n" + assertion;
  if (!extra.empty()) {
    msg +=
        "\n"
        "\n" +
        extra;
  }

  return msg;
}

std::string makeExpressionMessage(const std::string &file,
                                  int line,
                                  const std::string &assertion,
                                  const std::string &expansion) {
  return makeMessage(file, line, assertion, "Expands to:\n" + expansion);
}

std::string makeUnthrownExceptionMessage(const std::string &file,
                                         int line,
                                         const std::string &assertion) {
  return makeMessage(file, line, assertion, "No exception was thrown.");
}

std::string makeWrongExceptionMessage(const std::string &file,
                                      int line,
                                      const std::string &assertion,
                                      const std::string &expected) {
  return makeMessage(file,
                     line,
                     assertion,
                     "Thrown exception did not match " + expected + ".");
}

} // namespace detail
} // namespace rc

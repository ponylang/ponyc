#pragma once

namespace rc {
namespace detail {

std::string makeMessage(const std::string &file,
                        int line,
                        const std::string &assertion,
                        const std::string &extra = "");

std::string makeExpressionMessage(const std::string &file,
                                  int line,
                                  const std::string &assertion,
                                  const std::string &expansion);

std::string makeUnthrownExceptionMessage(const std::string &file,
                                         int line,
                                         const std::string &assertion);

std::string makeWrongExceptionMessage(const std::string &file,
                                      int line,
                                      const std::string &assertion,
                                      const std::string &expected);

template <typename Expression>
void doAssert(const Expression &expression,
              bool expectedResult,
              CaseResult::Type type,
              const std::string &file,
              int line,
              const std::string &assertion) {
  if (static_cast<bool>(expression.value()) != expectedResult) {
    std::ostringstream ss;
    expression.show(ss);
    throw CaseResult(type,
                     makeExpressionMessage(file, line, assertion, ss.str()));
  }
}

} // namespace detail
} // namespace rc

#include "rapidcheck/detail/Platform.h"

#ifndef _MSC_VER
#include <cxxabi.h>
#endif // _MSC_VER

#include <cstdlib>

namespace rc {
namespace detail {

#ifdef _MSC_VER

namespace {

bool replaceFirst(std::string &str,
                  const std::string &find,
                  const std::string &replacement) {
  const auto pos = str.find(find);
  if (pos == std::string::npos) {
    return false;
  }

  const auto it = begin(str) + pos;
  str.replace(it, it + find.size(), replacement);
  return true;
}

bool replaceAll(std::string &str,
                const std::string &find,
                const std::string &replacement) {
  bool replacedAny = false;
  while (const auto didReplace = replaceFirst(str, find, replacement)) {
    replacedAny = replacedAny || didReplace;
  }
  return replacedAny;
}

} // namespace

std::string demangle(const char *name) {
  // Class names are given as "class MyClass" or "struct MyStruct" and we don't
  // want that so we're gonna strip that away.
  auto str = std::string(name);
  replaceAll(str, "__int64", "long long");
  replaceAll(str, "class ", "");
  replaceAll(str, "struct ", "");
  replaceAll(str, "enum ", "");
  return str;
}

Maybe<std::string> getEnvValue(const std::string &name) {
  char *buffer = nullptr;
  _dupenv_s(&buffer, nullptr, name.c_str());
  const auto ptr = std::unique_ptr<char, decltype(&std::free)>(buffer, &std::free);

  if (buffer != nullptr) {
    return std::string(buffer);
  } else {
    return Nothing;
  }
}

#else // _MSC_VER

std::string demangle(const char *name) {
  std::string demangled(name);
  int status;
  char *buf = abi::__cxa_demangle(name, 0, 0, &status);
  if (status == 0) {
    demangled = std::string(buf);
  }
  free(buf);
  return demangled;
}

Maybe<std::string> getEnvValue(const std::string &name) {
  const auto value = std::getenv(name.c_str());
  if (value != nullptr) {
    return std::string(value);
  } else {
    return Nothing;
  }
}

#endif // _MSC_VER

} // namespace detail
} // namespace rc

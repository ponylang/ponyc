#include "MapParser.h"

#include <cctype>
#include <algorithm>
#include <locale>

#include "ParseException.h"

namespace rc {
namespace detail {

struct ParseState {
  const std::string *str;
  std::string::size_type pos;

  char c() const { return (*str)[pos]; }
  bool end() const { return pos >= str->size(); }
};

namespace {

bool isQuote(char c) { return (c == '\'') || (c == '\"'); }

template <typename Pred>
bool takeWhile(ParseState &state, std::string &result, const Pred &pred) {
  const auto start = state.pos;
  while (!state.end() && pred(state.c())) {
    state.pos++;
  }

  result = state.str->substr(start, state.pos - start);
  return true;
}

bool skipSpace(ParseState &state) {
  std::string space;
  return takeWhile(
      state,
      space,
      [](char c) { return std::isspace(c, std::locale::classic()); });
}

bool parseQuotedString(ParseState &state, std::string &value) {
  char quote = state.c();
  if (!isQuote(quote)) {
    return false;
  }
  state.pos++;

  value = std::string();
  bool escape = false;
  while (!state.end()) {
    char c = state.c();
    if (!escape && (c == '\\')) {
      escape = true;
    } else if (!escape && (c == quote)) {
      state.pos++;
      return true;
    } else {
      value += c;
      escape = false;
    }

    state.pos++;
  }

  throw ParseException(state.pos, "Unexpected end in quoted string");
}

template <typename Pred>
bool parseString(ParseState &state, std::string &value, Pred pred) {
  if (state.end()) {
    value = std::string();
    return true;
  }

  if (!parseQuotedString(state, value)) {
    takeWhile(state, value, pred);
  }
  return true;
}

bool parsePair(ParseState &s0, std::pair<std::string, std::string> &pair) {
  ParseState s1(s0);
  skipSpace(s1);
  parseString(s1,
              pair.first,
              [](char c) {
                return (c != '=') && !std::isspace(c, std::locale::classic());
              });
  if (pair.first.empty()) {
    return false;
  }

  skipSpace(s1);
  if (s1.end() || (s1.c() != '=')) {
    // Have no value, but that's okay, set to empty.
    pair.second = std::string();
  } else {
    // Have value, parse it.
    s1.pos++;
    skipSpace(s1);
    parseString(
        s1,
        pair.second,
        [](char c) { return !std::isspace(c, std::locale::classic()); });
  }

  s0 = s1;
  return true;
}

} // namespace

std::map<std::string, std::string> parseMap(const std::string &str) {
  std::map<std::string, std::string> config;
  ParseState state;
  state.str = &str;
  state.pos = 0;

  std::pair<std::string, std::string> pair;
  while (parsePair(state, pair)) {
    config[pair.first] = pair.second;
  }

  return config;
}

namespace {

std::string quoteString(const std::string &str, char quoteChar) {
  std::string escaped;
  for (const char c : str) {
    if ((c == quoteChar) || (c == '\\')) {
      escaped += '\\';
    }
    escaped += c;
  }

  return quoteChar + escaped + quoteChar;
}

std::string maybeQuoteString(const std::string &str, bool doubleQuote) {
  if (str.empty()) {
    return "\"\"";
  }

  bool hasSpecialChar =
      std::any_of(begin(str),
                  end(str),
                  [](char c) {
                    return std::isspace(c, std::locale::classic()) ||
                        isQuote(c) || (c == '=') || (c == '\\');
                  });

  char quoteChar = doubleQuote ? '"' : '\'';
  return hasSpecialChar ? quoteString(str, quoteChar) : str;
}

std::string pairToString(const std::pair<std::string, std::string> &pair,
                         bool doubleQuote) {
  if (pair.second.empty()) {
    return maybeQuoteString(pair.first, doubleQuote);
  } else {
    return maybeQuoteString(pair.first, doubleQuote) + '=' +
        maybeQuoteString(pair.second, doubleQuote);
  }
}

} // namespace

std::string mapToString(const std::map<std::string, std::string> &map,
                        bool doubleQuote) {
  std::string str;
  auto it = begin(map);
  if (it == end(map)) {
    return str;
  }
  str += pairToString(*it, doubleQuote);
  for (it++; it != end(map); it++) {
    str += " " + pairToString(*it, doubleQuote);
  }

  return str;
}

} // namespace detail
} // namespace rc

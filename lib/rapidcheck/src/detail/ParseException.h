#pragma once

#include <stdexcept>
#include <string>

namespace rc {
namespace detail {

class ParseException : public std::exception {
public:
  /// C-tor.
  ///
  /// @param pos  The position at which the parse error occured.
  /// @param msg  A message describing the parse error.
  ParseException(std::string::size_type pos, const std::string &msg);

  /// Returns the position.
  std::string::size_type position() const;

  /// Returns the message.
  std::string message() const;

  const char *what() const noexcept override;

private:
  std::string::size_type m_pos;
  std::string m_msg;
  std::string m_what;
};

} // namespace detail
} // namespace rc

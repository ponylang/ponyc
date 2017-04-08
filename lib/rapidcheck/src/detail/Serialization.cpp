#include "rapidcheck/detail/Serialization.h"

namespace rc {
namespace detail {

SerializationException::SerializationException(const std::string &msg)
    : m_msg(msg) {}

std::string SerializationException::message() const { return m_msg; }

const char *SerializationException::what() const noexcept {
  return m_msg.c_str();
}

} // namespace detail
} // namespace rc

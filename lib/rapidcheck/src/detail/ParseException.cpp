#include "ParseException.h"

namespace rc {
namespace detail {

ParseException::ParseException(std::string::size_type pos,
                               const std::string &msg)
    : m_pos(pos)
    , m_msg(msg)
    , m_what("@" + std::to_string(m_pos) + ": " + msg) {}

std::string::size_type ParseException::position() const { return m_pos; }

std::string ParseException::message() const { return m_msg; }

const char *ParseException::what() const noexcept { return m_what.c_str(); }

} // namespace detail
} // namespace rc

#pragma once

namespace rc {
namespace detail {

/// Returns the current logging stream.
std::ostream &log();

/// Logs the given message.
void log(const std::string &msg);

} // namespace detail
} // namespace rc

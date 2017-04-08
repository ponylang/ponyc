#include "rapidcheck/detail/Any.h"

namespace rc {
namespace detail {

Any::Any() noexcept {}

void Any::reset() { m_impl.reset(); }


void Any::showType(std::ostream &os) const {
  if (m_impl) {
    m_impl->showType(os);
  }
}

void Any::showValue(std::ostream &os) const {
  if (m_impl) {
    m_impl->showValue(os);
  }
}

Any::operator bool() const { return static_cast<bool>(m_impl); }

std::ostream &operator<<(std::ostream &os, const Any &value) {
  value.showValue(os);
  return os;
}

} // namespace detail
} // namespace rc

#include "rapidcheck/detail/ImplicitParam.h"

namespace rc {
namespace detail {

ImplicitScope::ImplicitScope() { m_scopes.emplace(); }

ImplicitScope::~ImplicitScope() {
  for (auto destructor : m_scopes.top()) {
    destructor();
  }
  m_scopes.pop();
}

ImplicitScope::ScopeStack ImplicitScope::m_scopes;

} // namespace detail
} // namespace rc

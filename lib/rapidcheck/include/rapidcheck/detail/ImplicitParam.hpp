#pragma once

#include <iostream>
#include "rapidcheck/Show.h"

namespace rc {
namespace detail {

template <typename Param>
ImplicitParam<Param>::ImplicitParam(ValueType value) {
  m_stack.push(
      std::make_pair(std::move(value), ImplicitScope::m_scopes.size()));
}

template <typename StackT, StackT *stack>
void popStackBinding() {
  stack->pop();
}

template <typename Param>
typename ImplicitParam<Param>::ValueType &ImplicitParam<Param>::value() {
  const auto scopeLevel = ImplicitScope::m_scopes.size();
  if (m_stack.empty() || (m_stack.top().second < scopeLevel)) {
    m_stack.push(
        std::make_pair(Param::defaultValue(), ImplicitScope::m_scopes.size()));
    if (!ImplicitScope::m_scopes.empty()) {
      ImplicitScope::m_scopes.top().push_back(
          popStackBinding<StackT, &m_stack>);
    }
  }

  return m_stack.top().first;
}

template <typename Param>
ImplicitParam<Param>::~ImplicitParam() {
  m_stack.pop();
}

template <typename Param>
typename ImplicitParam<Param>::StackT ImplicitParam<Param>::m_stack;

} // namespace detail
} // namespace rc

#pragma once

#include <string>

#include "rapidcheck/Show.h"

#define RC_FOREACH_OP(RC_F)                                                    \
  RC_F(Mul, *)                                                                 \
  RC_F(Div, / )                                                                \
  RC_F(Rem, % )                                                                \
  RC_F(Plus, +)                                                                \
  RC_F(Minus, -)                                                               \
  RC_F(LShift, << )                                                            \
  RC_F(RShift, >> )                                                            \
  RC_F(LT, < )                                                                 \
  RC_F(GT, > )                                                                 \
  RC_F(GTEq, >= )                                                              \
  RC_F(LTEq, <= )                                                              \
  RC_F(Eq, == )                                                                \
  RC_F(NEq, != )                                                               \
  RC_F(And, &)                                                                 \
  RC_F(XOR, ^)                                                                 \
  RC_F(OR, | )                                                                 \
  RC_F(BoolAND, &&)                                                            \
  RC_F(BoolOR, || )

namespace rc {
namespace detail {
namespace expr {

#define RC_FWD_OP_CLASS(Name, op)                                              \
  template <typename LExpr, typename RHS>                                      \
  class Name;

RC_FOREACH_OP(RC_FWD_OP_CLASS)

#undef RC_FWD_OP_CLASS

template <typename This>
class Expr {
public:
#define RC_OP_OVERLOAD(Name, op)                                               \
  template <typename RHS>                                                      \
  expr::Name<This, RHS> operator op(const RHS &rhs) const {                    \
    return expr::Name<This, RHS>(*static_cast<const This *>(this), rhs);       \
  }

  RC_FOREACH_OP(RC_OP_OVERLOAD)

#undef RC_OP_OVERLOAD
};

template <typename T>
class Value : public Expr<Value<T>> {
public:
  using ValueType = T;

  explicit Value(const T &value)
      : m_value(value) {}

  const ValueType &value() const { return m_value; }
  void show(std::ostream &os) const { ::rc::show(m_value, os); }

private:
  const T &m_value;
};

#define RC_OP_CLASS(Name, op)                                                  \
  template <typename LExpr, typename RHS>                                      \
  class Name : public Expr<Name<LExpr, RHS>> {                                 \
  public:                                                                      \
    using ValueType = decltype(std::declval<typename LExpr::ValueType>()       \
                                   op std::declval<RHS>());                    \
                                                                               \
    Name(const LExpr &expr, const RHS &rhs)                                    \
        : m_lexpr(expr)                                                        \
        , m_rhs(rhs) {}                                                        \
                                                                               \
    ValueType value() const { return m_lexpr.value() op m_rhs; }               \
    void show(std::ostream &os) const {                                        \
      m_lexpr.show(os);                                                        \
      os << " " #op " ";                                                       \
      ::rc::show(m_rhs, os);                                                   \
    }                                                                          \
                                                                               \
  private:                                                                     \
    const LExpr &m_lexpr;                                                      \
    const RHS &m_rhs;                                                          \
  };

RC_FOREACH_OP(RC_OP_CLASS)

#undef RC_OP_CLASS

struct Seed {
  template <typename T>
  Value<T> operator->*(const T &value) const {
    return Value<T>(value);
  }
};

} // namespace expr

#undef RC_FOREACH_OP

/// This is the macro to use. Captures the given expression into an object that
/// has two methods, `value()` and `show()`. The former returns the value of the
/// expression while the latter outputs the expansion of the expression to a
/// given `std::ostream`.
///
/// The neat thing about this solution is that you can get both the value and
/// the expansion of the expression without evaluating any component more than
/// once.
#define RC_INTERNAL_CAPTURE(expression)                                        \
  (::rc::detail::expr::Seed()->*expression)

} // namespace detail
} // namespace rc

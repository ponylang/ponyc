#ifndef PLATFORM_SIGNED_H
#define PLATFORM_SIGNED_H

#define STATIC_ERROR_INT_COPY \
  "[BigSigned(const T& value)]: Argument 'value' is not arithmetic!"

#include <type_traits>
#include "unsigned.h"

typedef enum sign_t
{
  negative = -1,
  zero = 0,
  positive = 1,
} sign_t;

class SignedInt128
{
public:
  sign_t sign;
  UnsignedInt128 magnitude;

  SignedInt128() {}
  SignedInt128(const SignedInt128& val)
  {
    sign = val.sign;
    magnitude = val.magnitude;
  }
  SignedInt128(const UnsignedInt128& val) : magnitude(val)
  {
    sign = (magnitude == 0) ? zero : positive;
  }

  template<typename T>
  SignedInt128(const T& val) : magnitude(val)
  {
    static_assert(std::is_arithmetic<T>::value, STATIC_ERROR_INT_COPY);

    if(val == 0)
    {
      sign = zero;
      return;
    }

    sign = (val < 0) ? negative : positive;
  }

  SignedInt128& operator=(const SignedInt128& rvalue);
  SignedInt128& operator=(const UnsignedInt128& rvalue);
  SignedInt128& operator+=(const SignedInt128& rvalue);
  SignedInt128& operator-=(const SignedInt128& rvalue);
  SignedInt128& operator*=(const SignedInt128& rvalue);
  SignedInt128& operator/=(const SignedInt128& rvalue);
  SignedInt128& operator%=(const SignedInt128& rvalue);
  SignedInt128& operator&=(const SignedInt128& rvalue);
  SignedInt128& operator|=(const SignedInt128& rvalue);
  SignedInt128& operator^=(const SignedInt128& rvalue);
  SignedInt128& operator<<=(const int shift);
  SignedInt128& operator>>=(const int shift);
};

cmp_t compare(const SignedInt128& lvalue, const SignedInt128& rvalue);

bool operator==(const SignedInt128& lvalue, const SignedInt128& rvalue);
bool operator!=(const SignedInt128& lvalue, const SignedInt128& rvalue);
bool operator<(const SignedInt128& lvalue, const SignedInt128& rvalue);
bool operator<=(const SignedInt128& lvalue, const SignedInt128& rvalue);
bool operator>(const SignedInt128& lvalue, const SignedInt128& rvalue);
bool operator>=(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator+(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator-(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator*(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator/(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator%(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator&(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator|(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator^(const SignedInt128& lvalue, const SignedInt128& rvalue);
SignedInt128 operator<<(const SignedInt128& lvalue, const int shift);
SignedInt128 operator>>(const SignedInt128& lvalue, const int shift);
SignedInt128 operator~(const SignedInt128& lvalue);
SignedInt128 operator++(const SignedInt128& lvalue);
SignedInt128 operator++(const SignedInt128& lvalue, int);
SignedInt128& operator--(SignedInt128& rvalue);
SignedInt128 operator--(SignedInt128& lvalue, int);
SignedInt128 operator-(const UnsignedInt128& rvalue);

#endif

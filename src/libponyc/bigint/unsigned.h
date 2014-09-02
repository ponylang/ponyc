#ifndef BIGINT_UNSIGNED_H
#define BIGINT_UNSIGNED_H

#define STATIC_ERROR_UINT_CONV \
  "[BigUnsigned(const T& value)]: Argument 'value' is not arithmetic!"

#define STATIC_ERROR_UINT_TYPE_CAST \
  "[BigUnsigned T()]: Casting is only supported for builtin arithmetic types!"

#include <type_traits>
#include "base.h"

class UnsignedInt128
{
public:
  uint64_t low;
  uint64_t high;

  UnsignedInt128() : low(0), high(0) {};
  UnsignedInt128(const UnsignedInt128& value) 
  {
    low = value.low;
    high = value.high;
  }
 
  template<typename T>
  UnsignedInt128(const T& value) : UnsignedInt128()
  { 
    static_assert(std::is_arithmetic<T>::value, STATIC_ERROR_UINT_CONV);

    if (value >= 0)
      low = (uint64_t)value;
    else
      low = UINT64_MAX;
  }
  
  template<typename T>
  explicit operator T()
  {
    static_assert(std::is_arithmetic<T>::value, STATIC_ERROR_UINT_TYPE_CAST);
    return (T)low;
  }

  UnsignedInt128& operator=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator+=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator-=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator*=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator/=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator%=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator&=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator|=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator^=(const UnsignedInt128& rvalue);
  UnsignedInt128& operator<<=(const int shift);
  UnsignedInt128& operator>>=(const int shift);
};

cmp_t compare(const UnsignedInt128& lvalue, const UnsignedInt128& rvalue);
bool operator==(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
bool operator!=(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
bool operator<(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
bool operator<=(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
bool operator>(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
bool operator>=(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
UnsignedInt128 operator+(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
UnsignedInt128 operator-(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
UnsignedInt128 operator*(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
UnsignedInt128 operator/(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
UnsignedInt128 operator%(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
UnsignedInt128 operator&(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
UnsignedInt128 operator|(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue);
UnsignedInt128 operator^(const UnsignedInt128& lvalue, 
  const UnsignedInt128& rvalue);

UnsignedInt128 operator<<(UnsignedInt128& lvalue, const int shift);
UnsignedInt128 operator>>(UnsignedInt128& lvalue, const int shift);
UnsignedInt128 operator~(const UnsignedInt128& lvalue);
UnsignedInt128 operator++(UnsignedInt128& rvalue);
UnsignedInt128 operator--(UnsignedInt128& lvalue, int);

#endif
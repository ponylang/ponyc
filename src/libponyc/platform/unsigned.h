#ifndef PLATFORM_UNSIGNED_H
#define PLATFORM_UNSIGNED_H

#define STATIC_ERROR_UINT_CONV \
  "[UnsignedInt128(const T& value)]: Argument 'value' is not arithmetic!"

#define STATIC_ERROR_UINT_TYPE_CAST \
  "[UnsignedInt128 T()]: Casting is only supported for builtin arithmetic types!"

#include <type_traits>
#include <stdexcept>

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

    if(value >= 0)
      low = (uint64_t)value;
    else
    {
      low = high = UINT64_MAX;

      if(value < -1)
        *this -= (value + 1);
    }
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
UnsignedInt128& operator++(UnsignedInt128& rvalue);
UnsignedInt128 operator++(UnsignedInt128& lvalue, int);
UnsignedInt128& operator--(UnsignedInt128& rvalue);
UnsignedInt128 operator--(UnsignedInt128& lvalue, int);

//useful constants
static UnsignedInt128 uint128_0 = 0;
static UnsignedInt128 uint128_1 = 1;
static UnsignedInt128 uint128_not_1 = ~uint128_1;

#endif
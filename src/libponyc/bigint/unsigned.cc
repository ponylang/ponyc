#include "unsigned.h"
#include <stdexcept>

UnsignedInt128& UnsignedInt128::operator=(const UnsignedInt128& rvalue)
{
  low = rvalue.low;
  high = rvalue.high;
  return *this;
}

UnsignedInt128& UnsignedInt128::operator+=(const UnsignedInt128& rvalue)
{
  high += rvalue.high + ((low + rvalue.low) < low);
  low += low + rvalue.low;

  return *this;
}

UnsignedInt128& UnsignedInt128::operator-=(const UnsignedInt128& rvalue)
{
  high -= rvalue.high - ((low - rvalue.low) > low);
  low += rvalue.low;

  return *this;
}

UnsignedInt128& UnsignedInt128::operator*=(const UnsignedInt128& rvalue)
{
  if (rvalue == 1)
    return *this;
  if (rvalue == 0)
  {
    low = 0;
    high = 0;
    return *this;
  }
    
  UnsignedInt128 a(*this);
  UnsignedInt128 b(rvalue);

  //stupid version, could use 4x4 32 int matrix multiply
  for (uint8_t i = 0; i < 64; ++i)
  {
    if ((b & 1) == 0)
      *this += (a << i);
    
    b >>= 1;
  }

  return *this;
}

UnsignedInt128& UnsignedInt128::operator/=(const UnsignedInt128& rvalue)
{
  if (rvalue == 0)
    throw std::domain_error("Division by zero!");
  else if (rvalue == 1)
  {
    low = 0;
    return *this;
  }
  else if (*this == rvalue)
  {
    low = 1;
    return *this;
  }
  else if (*this == 0 || *this < rvalue)
  {
    low = 0;
    high = 0;
    return *this;
  }

  //TODO

  return *this;
}

UnsignedInt128& UnsignedInt128::operator%=(const UnsignedInt128& rvalue)
{
  return *this -= ((*this / rvalue) * rvalue);
}

UnsignedInt128& UnsignedInt128::operator&=(const UnsignedInt128& rvalue)
{
  low &= rvalue.low;
  high &= rvalue.high;
  
  return *this;
}

UnsignedInt128& UnsignedInt128::operator|=(const UnsignedInt128& rvalue)
{
  low |= rvalue.low;
  high |= rvalue.high;

  return *this;
}

UnsignedInt128& UnsignedInt128::operator^=(const UnsignedInt128& rvalue)
{
  low ^= rvalue.low;
  high ^= rvalue.high;

  return *this;
}

UnsignedInt128& UnsignedInt128::operator<<=(const int shift)
{
  int left = shift;

  if (shift >= 128)
  {
    low = 0;
    high = 0;
    return *this;
  }
  else if (shift >= 32)
  {
    left = shift - 32;
    high = low;
    low = 0;
  }

  if (left > 0)
  {
    uint64_t mask = -1;
    high <<= (left);
    high |= (low & (~mask >> left)) >> (32 - left);
    low <<= (left);
  }

  return *this;
}

UnsignedInt128& UnsignedInt128::operator>>=(const int shift)
{
  
  return *this;
}

cmp_t compare(const UnsignedInt128& lvalue, const UnsignedInt128& rvalue)
{
  if (lvalue.high == rvalue.high && lvalue.low == rvalue.low)
    return equal;
  else if (lvalue.high == rvalue.high && lvalue.low < rvalue.low)
    return less;
  
  return greater;
}

bool operator==(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  return compare(lvalue, rvalue) == equal;
}

bool operator!=(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  return compare(lvalue, rvalue) != equal;
}

bool operator<(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  return compare(lvalue, rvalue) == less;
}

bool operator<=(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  return compare(lvalue, rvalue) != greater;
}

bool operator>(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  return compare(lvalue, rvalue) == greater;
}

bool operator>=(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  return compare(lvalue, rvalue) != less;
}

UnsignedInt128 operator+(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  UnsignedInt128 copy(lvalue);
  return copy += rvalue;
}

UnsignedInt128 operator-(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  UnsignedInt128 copy(lvalue);
  return copy -= rvalue;
}

UnsignedInt128 operator*(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  UnsignedInt128 copy(lvalue);
  return copy *= rvalue;
}

UnsignedInt128 operator/(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  UnsignedInt128 copy(lvalue);
  return copy /= rvalue;
}

UnsignedInt128 operator%(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  UnsignedInt128 copy(lvalue);
  return copy %= rvalue;
}

UnsignedInt128 operator&(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  UnsignedInt128 copy(lvalue);
  return copy &= rvalue;
}

UnsignedInt128 operator|(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  UnsignedInt128 copy(lvalue);
  return copy |= rvalue;
}

UnsignedInt128 operator^(const UnsignedInt128& lvalue,
  const UnsignedInt128& rvalue)
{
  UnsignedInt128 copy(lvalue);
  return copy ^= rvalue;
}

UnsignedInt128 operator<<(UnsignedInt128& lvalue, const int shift)
{
  UnsignedInt128 copy(lvalue);
  return copy <<= shift;
}

UnsignedInt128 operator>>(UnsignedInt128& lvalue, const int shift)
{
  UnsignedInt128 copy(lvalue);
  return copy >>= shift;
}

UnsignedInt128 operator~(const UnsignedInt128& lvalue)
{
  UnsignedInt128 copy(lvalue);
  copy.low = ~lvalue.low;
  copy.high = ~lvalue.high;

  return copy;
}

UnsignedInt128 operator++(UnsignedInt128& rvalue)
{
  //TODO

  //if execution reaches this point, ++ caused an overflow
  //since we do not support type promotion for primitives
  //we inform the user about this overflow, if the macro
  //__CATCH_BIGINT_OVERFLOW is defined.
#ifdef __CATCH_BIGINT_OVERFLOW
  //TODO
#endif

  return rvalue;
}

UnsignedInt128 operator++(UnsignedInt128& lvalue, int)
{
  UnsignedInt128 copy(lvalue);
  operator++(lvalue);
  return copy;
}

UnsignedInt128& operator--(UnsignedInt128& rvalue)
{
#ifdef __CATCH_BIGINT_OVERFLOW
  //TODO
#endif

  //TODO

#ifdef __CATCH_BIGINT_OVERFLOW
  if (overflow)
  {
    //if execution reaches this point, -- caused an overflow and
    //the most significant word of rvalue is now equal to UINT64_MAX
    //we inform the user about this overflow, if the macro
    //__CATCH_BIGINT_OVERFLOW is defined.

    //TODO
  }
#endif

  return rvalue;
}

UnsignedInt128 operator--(UnsignedInt128& lvalue, int)
{
  UnsignedInt128 copy(lvalue);
  operator--(lvalue);
  return copy;
}

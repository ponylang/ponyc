#include <unsigned.h>

UnsignedInt128& UnsignedInt128::operator=(const UnsignedInt128& rvalue)
{
  low = rvalue.low;
  high = rvalue.high;
  return *this;
}

UnsignedInt128& UnsignedInt128::operator+=(const UnsignedInt128& rvalue)
{
  high = high + rvalue.high + ((low + rvalue.low) < low);
  low += rvalue.low;
  return *this;
}

UnsignedInt128& UnsignedInt128::operator-=(const UnsignedInt128& rvalue)
{
  high = high - rvalue.high - ((low - rvalue.low) > low);
  low -= rvalue.low;
  return *this;
}

UnsignedInt128& UnsignedInt128::operator*=(const UnsignedInt128& rvalue)
{
  if(rvalue == uint128_1)
    return *this;
  if(rvalue == uint128_0)
  {
    low = 0;
    high = 0;
    return *this;
  }

  UnsignedInt128 a(*this); //not really want to copy this.
  UnsignedInt128 b(rvalue);

  low = high = 0;

  while(b > uint128_0)
  {
    if((b & uint128_1) != uint128_0)
      *this += a;

    a <<= 1;
    b >>= 1;
  }

  return *this;
}

UnsignedInt128& UnsignedInt128::operator/=(const UnsignedInt128& rvalue)
{
  if(rvalue == uint128_0)
    throw std::domain_error("Division by zero!");
  else if(rvalue == uint128_1)
  {
    return *this;
  }
  else if(*this == rvalue)
  {
    low = 1;
    high = 0;
    return *this;
  }
  else if(*this == uint128_0 || *this < rvalue)
  {
    low = 0;
    high = 0;
    return *this;
  }

  UnsignedInt128 q = 0;
  UnsignedInt128 r = 0;

  for(uint8_t i = 127; i < UINT8_MAX; --i)
  {
    r <<= 1;
    r |= (*this >> i) & uint128_1;

    if (r >= rvalue)
    {
      r -= rvalue;
      q |= (uint128_1 << i);
    }
  }

  //remainder is in variable r, may be used later
  //for better %= implementation, based on std::pair.
  return *this = q;
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
  if(shift == 0)
  {
    return *this;
  }
  else if(shift >= 128)
  {
    low = 0;
    high = 0;
  }
  else if(shift == 64)
  {
    high = low;
    low = 0;
  }
  else if(shift < 64)
  {
    high = (high << shift) + (low >> (64 - shift));
    low <<= shift;
  }
  else if((128 > shift) && (shift > 64))
  {
    high = low << (shift - 64);
    low = 0;
  }

  return *this;
}

UnsignedInt128& UnsignedInt128::operator>>=(const int shift)
{
  if(shift == 0)
  {
    return *this;
  }
  if(shift >= 128)
  {
    low = 0;
    high = 0;
  }
  else if(shift == 64)
  {
    low = high;
    high = 0;
  }
  else if(shift < 64)
  {
    high >>= shift;
    low = (high << (64 - shift)) + (low >> shift);
  }
  else if((128 > shift) && (shift > 64))
  {
    low = high >> (shift - 64);
    high = 0;
  }

  return *this;
}

cmp_t compare(const UnsignedInt128& lvalue, const UnsignedInt128& rvalue)
{
  if(lvalue.high == rvalue.high && lvalue.low == rvalue.low)
    return equal;
  else if(lvalue.high == rvalue.high && lvalue.low < rvalue.low)
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

UnsignedInt128& operator++(UnsignedInt128& rvalue)
{
  if (++rvalue.low == 0)
    ++rvalue.high;

#ifdef __CATCH_BIGINT_OVERFLOW
  //if execution reaches this point, ++ caused an overflow
  //since we do not support type promotion for primitives
  //we inform the user about this overflow, if the macro
  //__CATCH_BIGINT_OVERFLOW is defined.
  if(rvalue.high == 0)
  {
    //TODO
  }
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
  if (rvalue.low-- == 0)
    --rvalue.high;

#ifdef __CATCH_BIGINT_OVERFLOW
    //if execution reaches this point, -- caused an overflow and
    //the most significant word of rvalue is now equal to UINT64_MAX
    //we inform the user about this overflow, if the macro
    //__CATCH_BIGINT_OVERFLOW is defined.

    //TODO
#endif

  return rvalue;
}

UnsignedInt128 operator--(UnsignedInt128& lvalue, int)
{
  UnsignedInt128 copy(lvalue);
  operator--(lvalue);
  return copy;
}

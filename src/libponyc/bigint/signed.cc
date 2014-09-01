#include "signed.h"

SignedInt128& SignedInt128::operator=(const SignedInt128& rvalue)
{
  return *this;
}

SignedInt128& SignedInt128::operator=(const UnsignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator+=(const SignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator-=(const SignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator*=(const SignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator/=(const SignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator%=(const SignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator&=(const SignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator|=(const SignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator^=(const SignedInt128& rvalue)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator<<=(const int shift)
{
  //REMOVED
  return *this;
}

SignedInt128& SignedInt128::operator>>=(const int shift)
{
  //REMOVED
  return *this;
}

cmp_t compare(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  /*if (lvalue.sign < rvalue.sign)
  return less;
  else if (lvalue.sign > rvalue.sign)
  return greater;
  else
  {
  switch (sign)
  {
  case zero:
  return equal;
  case negative:
  return cmp_t(-compare(lvalue.magnitude, rvalue.magnitude));
  case positive:
  return compare(lvalue.magnitude, rvalue.magnitude);
  }
  }*/

  return invalid;
}

bool operator==(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  return (lvalue.sign == rvalue.sign) &&
    (lvalue.magnitude == rvalue.magnitude);
}

bool operator!=(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  return !operator==(lvalue, rvalue);
}

bool operator<(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  return compare(lvalue, rvalue) == less;
}

bool operator<=(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  return compare(lvalue, rvalue) != greater;
}

bool operator>(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  return compare(lvalue, rvalue) == greater;
}

bool operator>=(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  return compare(lvalue, rvalue) != less;
}

SignedInt128 operator+(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  SignedInt128 copy(lvalue);
  return copy += rvalue;
}

SignedInt128 operator-(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  SignedInt128 copy(lvalue);
  return copy -= rvalue;
}

SignedInt128 operator*(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  SignedInt128 copy(lvalue);
  return copy *= rvalue;
}

SignedInt128 operator/(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  SignedInt128 copy(lvalue);
  return copy /= rvalue;
}

SignedInt128 operator%(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  SignedInt128 copy(lvalue);
  return copy %= rvalue;
}

SignedInt128 operator&(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  SignedInt128 copy(lvalue);
  return copy %= rvalue;
}

SignedInt128 operator|(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  SignedInt128 copy(lvalue);
  return copy |= rvalue;
}

SignedInt128 operator^(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  SignedInt128 copy(lvalue);
  return copy ^= rvalue;
}

SignedInt128 operator<<(const SignedInt128& lvalue, const int shift)
{
  SignedInt128 copy(lvalue);
  return copy <<= shift;
}

SignedInt128 operator>>(const SignedInt128& lvalue, const int shift)
{
  SignedInt128 copy(lvalue);
  return copy >>= shift;
}

SignedInt128 operator~(const SignedInt128& lvalue)
{
  //REMOVED
  return lvalue;
}

SignedInt128 operator++(const SignedInt128& lvalue)
{
  //REMOVED
  return lvalue;
}

SignedInt128 operator++(const SignedInt128& lvalue, int)
{
  SignedInt128 copy(lvalue);
  operator++(lvalue);
  return copy;
}

SignedInt128& operator--(SignedInt128& rvalue)
{
  //REMOVED
  return rvalue;
}

SignedInt128 operator--(SignedInt128& lvalue, int)
{
  SignedInt128 copy(lvalue);
  operator--(lvalue);
  return copy;
}


SignedInt128& operator-(const UnsignedInt128& rvalue)
{
  //simple 2's complement
  return SignedInt128(operator~(rvalue)) += 1;
}
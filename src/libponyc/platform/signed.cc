#include <signed.h>

SignedInt128& SignedInt128::operator=(const SignedInt128& rvalue)
{
  sign = rvalue.sign;
  magnitude = rvalue.magnitude;
  return *this;
}

SignedInt128& SignedInt128::operator=(const UnsignedInt128& rvalue)
{
  sign = positive;
  magnitude = rvalue;
  return *this;
}

SignedInt128& SignedInt128::operator+=(const SignedInt128& rvalue)
{
  if(sign == zero)
    magnitude = rvalue.magnitude;
  else if(rvalue.sign == zero)
    return *this;
  else if(sign == rvalue.sign)
    magnitude += rvalue.magnitude;
  else
  {
    //signs must be different (one is negative, one positive)
    switch(compare(magnitude, rvalue.magnitude))
    {
      case equal:
        sign = zero;
        magnitude = 0;
        break;
      case greater: //rvalue is negative
        magnitude -= rvalue.magnitude;
        break;
      case less: //this is negative
        sign = rvalue.sign;
        magnitude = rvalue.magnitude - magnitude;
        break;
    }
  }

  return *this;
}

SignedInt128& SignedInt128::operator-=(const SignedInt128& rvalue)
{
  if(rvalue.sign == zero)
    return *this;
  else if(sign == zero)
  {
    magnitude = rvalue.magnitude;
    sign = sign_t(-rvalue.sign);
  }
  else
  {
    switch(compare(magnitude, rvalue.magnitude))
    {
      case equal:
        magnitude = 0;
        sign = zero;
        break;
      case greater:
        magnitude -= rvalue.magnitude;
        break;
      case less:
        sign = sign_t(-rvalue.sign);
        magnitude = rvalue.magnitude - magnitude;
        break;
    }
  }

  return *this;
}

SignedInt128& SignedInt128::operator*=(const SignedInt128& rvalue)
{
  if(sign == zero || rvalue.sign == zero)
  {
    sign = zero;
    magnitude = 0;
    return *this;
  }

  sign = (sign == rvalue.sign) ? positive : negative;
  magnitude *= rvalue.magnitude;

  return *this;
}

SignedInt128& SignedInt128::operator/=(const SignedInt128& rvalue)
{
  //TODO
  return *this;
}

SignedInt128& SignedInt128::operator%=(const SignedInt128& rvalue)
{
  //TODO
  return *this;
}

SignedInt128& SignedInt128::operator&=(const SignedInt128& rvalue)
{
  //TODO
  return *this;
}

SignedInt128& SignedInt128::operator|=(const SignedInt128& rvalue)
{
  //TODO
  return *this;
}

SignedInt128& SignedInt128::operator^=(const SignedInt128& rvalue)
{
  //TODO
  return *this;
}

SignedInt128& SignedInt128::operator<<=(const int shift)
{
  //TODO
  return *this;
}

SignedInt128& SignedInt128::operator>>=(const int shift)
{
  //TODO
  return *this;
}

cmp_t compare(const SignedInt128& lvalue, const SignedInt128& rvalue)
{
  if(lvalue.sign < rvalue.sign)
    return less;
  else if(lvalue.sign > rvalue.sign)
    return greater;
  else
  {
    switch(lvalue.sign)
    {
      case zero:
        return equal;
      case positive:
        return compare(lvalue.magnitude, rvalue.magnitude);
      case negative:
        return cmp_t(-compare(lvalue.magnitude, rvalue.magnitude));
    }
  }
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
  //TODO
  return lvalue;
}

SignedInt128 operator++(const SignedInt128& lvalue)
{
  //TODO
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
  //TODO
  return rvalue;
}

SignedInt128 operator--(SignedInt128& lvalue, int)
{
  SignedInt128 copy(lvalue);
  operator--(lvalue);
  return copy;
}


SignedInt128 operator-(const UnsignedInt128& rvalue)
{
  SignedInt128 n(rvalue);
  n.sign = negative;

  return n;
}

#ifndef BIGINT_SIGNED_H
#define BIGINT_SIGNED_H

#define STATIC_ERROR_INT_COPY \
  "[BigSigned(const T& value)]: Argument 'value' is not arithmetic!"

#include "unsigned.h"
#include <type_traits>

template<size_t words>
class BigSignedInt
{
protected:
  sign_t sign;
  BigUnsignedInt<words> magnitude;

public:
  BigSignedInt() {}
  BigSignedInt(const BigSignedInt<words>& val) {}
  BigSignedInt(const BigUnsignedInt<words>& val) : magnitude(val)
  {
    sign = (magnitude == 0) ? zero : positive;
  }

  template<typename T>
  BigSignedInt(const T& val)
  {
    static_assert(std::is_arithmetic<T>::value, STATIC_ERROR_INT_COPY);
    
    if (val == 0)
    {
      sign = zero;
      return;
    }
    
    //REMOVED

    sign = (val < 0) ? negative : positive;
  }

  BigSignedInt& operator=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator=(const BigUnsignedInt<words>& rvalue)
  {
    //REMOVED
    return *this;
  }
  
  BigSignedInt& operator+=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator-=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator*=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator/=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator%=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator&=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator|=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator^=(const BigSignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator<<=(const int shift)
  {
    //REMOVED
    return *this;
  }

  BigSignedInt& operator>>=(const int shift)
  {
    //REMOVED
    return *this;
  }

  bool operator==(const BigSignedInt& rvalue)
  {
    return (this->sign == rvalue.sign) && 
           (this->magnitude == rvalue.magnitude);
  }

  bool operator!=(const BigSignedInt& rvalue)
  {
    return !operator==(rvalue);
  }

  bool operator<(const BigSignedInt& rvalue)
  {
    return compare(*this, rvalue) == less;
  }

  bool operator<=(const BigSignedInt& rvalue)
  {
    return compare(*this, rvalue) != greater;
  }

  bool operator>(const BigSignedInt& rvalue)
  {
    return compare(*this, rvalue) == greater;
  }

  bool operator>=(const BigSignedInt& rvalue)
  {
    return compare(*this, rvalue) != less;
  }

   BigSignedInt operator+(const BigSignedInt& rvalue)
  {
    BigSignedInt<words> copy(*this);
    return copy += rvalue;
  }

  BigSignedInt operator-(const BigSignedInt& rvalue)
  {
    BigSignedInt<words> copy(*this);
    return copy -= rvalue;
  }

  BigSignedInt operator*(const BigSignedInt& rvalue)
  {
    BigSignedInt<words> copy(*this);
    return copy *= rvalue;
  }

  BigSignedInt operator/(const BigSignedInt& rvalue)
  {
    BigSignedInt<words> copy(*this);
    return copy /= rvalue;
  }

  BigSignedInt operator%(const BigSignedInt& rvalue)
  {
    BigSignedInt<words> copy(*this);
    return copy %= rvalue;
  }

  BigSignedInt operator&(const BigSignedInt& rvalue)
  {
    BigSignedInt<words> copy(*this);
    return copy %= rvalue;
  }

  BigSignedInt operator|(const BigSignedInt& rvalue)
  {
    BigSignedInt<words> copy(*this);
    return copy |= rvalue;
  }

  BigSignedInt<words> operator^(const BigSignedInt& rvalue)
  {
    BigSignedInt<words> copy(*this);
    return copy ^= rvalue;
  }

  BigSignedInt operator<<(const int shift)
  {
    BigSignedInt<words> copy(*this);
    return copy <<= shift;
  }

  BigSignedInt operator>>(const int shift)
  {
    BigSignedInt<words> copy(*this);
    return copy >>= shift;
  }

  BigSignedInt operator~()
  {
    //REMOVED
    return *this;
  }

  BigSignedInt operator++()
  {
    //REMOVED
    return *this;
  }

  BigSignedInt operator++(int)
  {
    BigSignedInt<words> copy(*this);
    this->operator++();
    return copy;
  }

  BigSignedInt& operator--()
  {
    //REMOVED
    return *this;
  }

  BigSignedInt operator--(int)
  {
    BigSignedInt<words> copy(*this);
    this->operator--();
    return copy;
  }
};

template<size_t words>
inline bool compare(const BigSignedInt<words>& lvalue,
  const BigSignedInt<words>& rvalue)
{
  //REMOVED
  return equal;
}

template<size_t words>
BigSignedInt<words>& operator-(const BigUnsignedInt<words>& rvalue)
{
  //simple 2's complement
  return BigSignedInt<words>(rvalue.operator~()) += 1;
}

#endif
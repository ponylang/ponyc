#ifndef BIGINT_UNSIGNED_H
#define BIGINT_UNSIGNED_H

#define STATIC_ERROR_UINT_CONV \
  "[BigUnsigned(const T& value)]: Argument 'value' is not arithmetic!"

#define STATIC_ERROR_UINT_TYPE_CAST \
  "[BigUnsigned T()]: Casting is only supported for builtin arithmetic types!"

#include <type_traits>

template<size_t words>
class BigUnsignedInt : public IntArray<words>
{ 
public:
  BigUnsignedInt() : IntArray<words>() {}
  BigUnsignedInt(const BigUnsignedInt& value) : IntArray<words>(value) {}
 //BigUnsignedInt(const BigSignedInt<words>& value) {}
 
  template<typename T>
  BigUnsignedInt(const T& value) : BigUnsignedInt()
  { 
    static_assert(std::is_arithmetic<T>::value, STATIC_ERROR_UINT_CONV);

    if (value == 0)
      return;

    this->len = 1;
    this->data[0] = (uint64_t)value;
  }
  
  template<typename T>
  explicit operator T()
  {
    static_assert(std::is_arithmetic<T>::value, STATIC_ERROR_UINT_TYPE_CAST);
    return (T)this->data[0];
  }

  BigUnsignedInt& operator=(const BigUnsignedInt& rvalue)
  {
    IntArray<words>::operator=(rvalue);
    return *this;
  }
  
  BigUnsignedInt& operator+=(const BigUnsignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator-=(const BigUnsignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator*=(const BigUnsignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator/=(const BigUnsignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator%=(const BigUnsignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator&=(const BigUnsignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator|=(const BigUnsignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator^=(const BigUnsignedInt& rvalue)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator<<=(const int shift)
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt& operator>>=(const int shift)
  {
    //REMOVED
    return *this;
  }

  /** Although the following operators do not modify the receiver,
   *  we need to declare them in class-scope, as otherwise implicit
   *  type conversion using the converstion constructor above for
   *  primitive builtin types does not work (due to an added template).
   *
   *  Declaring these operators as "friend" allows us to provide both
   *  parameters implicitly, which we need in order to be able to use
   *  the implicit conversion constructors above.
   */
  friend bool operator==(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    return ((IntArray<words>)lvalue).operator==(rvalue);
  }

  friend bool operator!=(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    return ((IntArray<words>)lvalue).operator!=(rvalue);
  }

  friend bool operator<(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    return compare(lvalue, rvalue) == less;
  }
  
  friend bool operator<=(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    return compare(lvalue, rvalue) != greater;
  }
  
  friend bool operator>(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    return compare(lvalue, rvalue) == greater;
  }

  friend bool operator>=(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    return compare(lvalue, rvalue) != less;
  }

  friend BigUnsignedInt operator+(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    BigUnsignedInt<words> copy(lvalue);
    return copy += rvalue;
  }

  friend BigUnsignedInt operator-(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt<words>& rvalue)
  {
    BigUnsignedInt<words> copy(lvalue);
    return copy -= rvalue;
  }

  friend BigUnsignedInt operator*(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    BigUnsignedInt<words> copy(lvalue);
    return copy *= rvalue;
  }

  friend BigUnsignedInt operator/(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    BigUnsignedInt<words> copy(lvalue);
    return copy /= rvalue;
  }

  friend BigUnsignedInt operator%(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    BigUnsignedInt<words> copy(lvalue);
    return copy %= rvalue;
  }

  friend BigUnsignedInt operator&(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    BigUnsignedInt<words> copy(lvalue);
    return copy &= rvalue;
  }

  friend BigUnsignedInt operator|(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    BigUnsignedInt<words> copy(lvalue);
    return copy |= rvalue;
  }

  friend BigUnsignedInt operator^(const BigUnsignedInt& lvalue, 
    const BigUnsignedInt& rvalue)
  {
    BigUnsignedInt<words> copy(lvalue);
    return copy ^= rvalue;
  }

  BigUnsignedInt operator<<(const int shift)
  {
    BigUnsignedInt<words> copy(*this);
    return copy <<= shift;
  }

  BigUnsignedInt operator>>(const int shift)
  {
    BigUnsignedInt<words> copy(*this);
    return copy >>= shift;
  }

  BigUnsignedInt operator~() const
  {
    //REMOVED
    return *this;
  }

  BigUnsignedInt operator++()
  {
    for (size_t i = 0; i < this->len; ++i)
    {
      ++this->data[i];
      if (this->data[i] != 0)
        return *this;
    }

    //if execution reaches this point, ++ caused an overflow
    //since we do not support type promotion for primitives
    //we inform the user about this overflow, if the macro
    //__CATCH_BIGINT_OVERFLOW is defined.
  #ifdef __CATCH_BIGINT_OVERFLOW
    //REMOVED
  #endif

    return *this;
  }

  BigUnsignedInt operator++(int)
  {
    BigUnsignedInt<words> copy(*this);
    this->operator++();
    return copy;
  }

  BigUnsignedInt& operator--()
  {
  #ifdef __CATCH_BIGINT_OVERFLOW
    bool overflow = (this->len == 0);
  #endif

    bool carry = true;

    for (size_t i = 0; carry; ++i)
    {
      carry = (this->data[i] == 0);
      --this->data[i];
    }
   
    if (this->data[this->len - 1] == 0)
      --this->len;

  #ifdef __CATCH_BIGINT_OVERFLOW
    if (overflow)
    {
      //if execution reaches this point, -- caused an overflow and
      //the most significant word of rvalue is now equal to UINT64_MAX
      //we inform the user about this overflow, if the macro
      //__CATCH_BIGINT_OVERFLOW is defined.

      //REMOVED
    }
  #endif

    return *this;
  }

  BigUnsignedInt operator--(int)
  {
    BigUnsignedInt<words> copy(*this);
    this->operator--();
    return copy;
  }
};

template<size_t words>
cmp_t compare(const BigUnsignedInt<words>& lvalue,
  const BigUnsignedInt<words>& rvalue)
{
  if (lvalue.len < rvalue.len)
    return less;
  else if (lvalue.len > rvalue.len)
    return greater;
  else
  {
    for (size_t i = lvalue.len - 1; i > 0; --i)
    {
      if (lvalue.data[i] == rvalue.data[i])
        continue;
      else if (lvalue.data[i] > rvalue.data[i])
        return greater;
      else
        return less;
    }
  }

  return equal;
}

#endif
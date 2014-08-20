#if !defined(PLATFORM_INT128_H) && defined(PLATFORM_IS_VISUAL_STUDIO)
#define PLATFORM_INT128_H

template<size_t words>
class BigUnsignedInteger
{
public:
  size_t len; //actual length in blocks of uint64_ts.
  uint64_t data[words];

  BigUnsignedInteger()
  {
    len = 0;
    // We have to clear data, because we want to simulate
    // primitive behaviour of BigIntegers.
    memset(&data, 0, sizeof(data));
  }

  BigUnsignedInteger(const BigUnsignedInteger &x)
  {
    len = x.len;
    memcpy(&data, &x.data, sizeof(data));
  }

  template<typename T>
  BigUnsignedInteger(const T &x) : BigUnsignedInteger()
  {
    len = 0;
    data[0] = x;
  }

  BigUnsignedInteger& operator=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }
  
  template<typename T>
  BigUnsignedInteger& operator=(const T &rvalue)
  {
    data[0] = rvalue;
    return *this;
  }

  BigUnsignedInteger& operator&=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }

  BigUnsignedInteger& operator|=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }

  BigUnsignedInteger& operator^=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigUnsignedInteger& operator&=(const T &rvalue)
  {
    data[0] &= rvalue;
    return *this;
  }

  template<typename T>
  BigUnsignedInteger& operator|=(const T &rvalue)
  {
    data[0] |= rvalue;
    return *this;
  }

  template<typename T>
  BigUnsignedInteger& operator^=(const T &rvalue)
  {
    data[0] ^= rvalue;
    return *this;
  }

  BigUnsignedInteger& operator<<=(const BigUnsignedInteger &rvalue)
  {
    return this;
  }

  BigUnsignedInteger& operator>>=(const BigUnsignedInteger &rvalue)
  {
    return this;
  }

  template<typename T>
  BigUnsignedInteger& operator<<=(const T &rvalue)
  {
    *this = *this << BigUnsignedInteger(rvalue);
    return *this
  }

  template<typename T>
  BigUnsignedInteger& operator>>=(const T &rvalue)
  {
    *this = *this >> BigUnsignedInteger(rvalue);
    return *this;
  }
 
  BigUnsignedInteger& operator+=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }

  BigUnsignedInteger& operator-=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }

  BigUnsignedInteger& operator*=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }

  BigUnsignedInteger& operator/=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }

  BigUnsignedInteger& operator%=(const BigUnsignedInteger &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigUnsignedInteger operator+(const T &rvalue) const
  {
    return this;
  }

  template<typename T>
  BigUnsignedInteger& operator+=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigUnsignedInteger operator-=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigUnsignedInteger& operator*=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigUnsignedInteger& operator/=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigUnsignedInteger& operator%=(const T &rvalue)
  {
    return *this;
  }

  /** Typecasts.
   *
   */
  template<typename T>
  operator T()
  {
    return (T)data[0];
  }
};

typedef enum cmp_t { less = -1, equal = 0, greater = 1 } cmp_t;
typedef enum sign_t { negative = -1, zero = 0, positive = 1 } sign_t;

template<size_t words>
class BigSignedInteger
{
public:
  sign_t sign;
  BigUnsignedInteger<words> magnitude;

  BigSignedInteger()
  {
    sign = zero;
  }

  BigSignedInteger(const BigSignedInteger &x)
  {
    //TODO
  }
  
  template<typename T>
  BigSignedInteger(const T &x) : BigSignedInteger()
  {
     //TODO
  }

  BigSignedInteger& operator=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  BigSignedInteger& operator=(const BigUnsignedInteger<words> &rvalue)
  {
    //TODO
    return *this;
  }

  template<typename T>
  BigSignedInteger& operator=(const T &rvalue)
  {
    magnitude.data[0] = rvalue;
    return *this;
  }

  BigSignedInteger& operator&=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  BigSignedInteger& operator|=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  BigSignedInteger& operator^=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigSignedInteger& operator&=(const T &rvalue)
  {
    data[0] &= rvalue;
    return *this;
  }

  template<typename T>
  BigSignedInteger& operator|=(const T &rvalue)
  {
    data[0] |= rvalue;
    return *this;
  }

  template<typename T>
  BigSignedInteger& operator^=(const T &rvalue)
  {
    data[0] ^= rvalue;
    return *this;
  }

  BigSignedInteger& operator<<=(const BigSignedInteger &rvalue)
  {
    return this;
  }

  BigSignedInteger& operator>>=(const BigSignedInteger &rvalue)
  {
    return this;
  }

  template<typename T>
  BigSignedInteger& operator<<=(const T &rvalue)
  {
    *this = *this << BigSignedInteger(rvalue);
    return *this
  }

  template<typename T>
  BigSignedInteger& operator>>=(const T &rvalue)
  {
    *this = *this >> BigSignedInteger(rvalue);
    return *this;
  }

  BigSignedInteger& operator+=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  BigSignedInteger& operator-=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  BigSignedInteger& operator*=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  BigSignedInteger& operator/=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  BigSignedInteger& operator%=(const BigSignedInteger &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigSignedInteger operator+(const T &rvalue) const
  {
    return this;
  }

  template<typename T>
  BigSignedInteger& operator+=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigSignedInteger operator-=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigSignedInteger& operator*=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigSignedInteger& operator/=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  BigSignedInteger& operator%=(const T &rvalue)
  {
    return *this;
  }

  template<typename T>
  operator T()
  {
    return (T)data[0];
  }
};

/** Static helper functions
 *
 */
#pragma region "Helpers"

template<size_t words>
static cmp_t compare(const BigUnsignedInteger<words>* a, const BigUnsignedInteger<words>* b)
{
  if (a->len < b->len)
    return less;
  else if (a->len > b->len)
    return greater;
  else
  {
    size_t i = a->len - 1;

    while (i > 0)
    {
      if (a->data[i] == b->data[i])
        continue;
      else if (a->data[i] > b->data[i])
        return greater;
      else
        return less;
    }
  }

  return equal;
}

template<size_t words>
static cmp_t compare(const BigUnsignedInteger<words>* a, const BigSignedInteger<words>* b)
{
  return equal;
}

template<size_t words>
static cmp_t compare(const BigSignedInteger<words>* a, const BigSignedInteger<words>* b)
{
  return equal;
}

#pragma endregion

/** Operators that no not modify the state of the receiver.
 *
 */
#pragma region "UnsignedOperators"

template<size_t words>
BigUnsignedInteger<words> operator&(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words>
BigUnsignedInteger<words> operator|(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words>
BigUnsignedInteger<words> operator^(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words>
BigUnsignedInteger<words> operator~(const BigUnsignedInteger<words> &x)
{
  return x;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator&(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  BigUnsignedInteger n();
  n->data[0] = lvalue.data[0] & rvalue;
  return n;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator|(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  BigUnsignedInteger n(lvalue);
  n->data[0] = lvalue.data[0] | rvalue;
  return n;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator^(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  BigUnsignedInteger n(lvalue);
  n->data[0] = lvalue.data[0] ^ rvalue;
  return n;
}

template<size_t words>
BigUnsignedInteger<words> operator<<(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words>
BigUnsignedInteger<words> operator>>(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return this;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator<<(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return lvalue << BigUnsignedInteger(rvalue);
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator>>(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return lvalue >> BigUnsignedInteger(rvalue);
}

template<size_t words>
BigSignedInteger<words> operator-(BigUnsignedInteger<words>& rvalue)
{
  BigSignedInteger<words> n(~rvalue);
  return n += 1;
}

/** Boolean operators.
*
*/
template<size_t words>
bool operator==(const BigUnsignedInteger<words> &lvalue, const BigUnsignedInteger<words> &rvalue)
{
  return compare(&lvalue, &rvalue) == equal;
}

template<size_t words>
bool operator!=(const BigUnsignedInteger<words> &lvalue, const BigUnsignedInteger<words> &rvalue)
{
  return compare(&lvalue, &rvalue) != equal;
}

template<size_t words>
bool operator>(const BigUnsignedInteger<words> &lvalue, const BigUnsignedInteger<words> &rvalue)
{
  return compare(&lvalue, &rvalue) == greater;
}

template<size_t words>
bool operator<(const BigUnsignedInteger<words> &lvalue, const BigUnsignedInteger<words> &rvalue)
{
  return compare(&lvalue, &rvalue) == less;
}

template<size_t words>
bool operator>=(const BigUnsignedInteger<words> &lvalue, const BigUnsignedInteger<words> &rvalue)
{
  return compare(&lvalue, &rvalue) != less;
}

template<size_t words>
bool operator<=(const BigUnsignedInteger<words> &lvalue, const BigUnsignedInteger<words> &rvalue)
{
  return compare(&lvalue, &rvalue) != greater;
}
 
template<size_t words>
bool operator!(const BigUnsignedInteger<words> &value)
{
  for (size_t i = 0; i < value.len; ++i)
  {
    if (value.data[i])
      return false;
  }

  return true;
}

template<size_t words>
bool operator&&(const BigUnsignedInteger<words> &lvalue, const BigUnsignedInteger<words> &rvalue)
{
  for (size_t i = 0; i < lvalue.len; ++i)
  {
    if (!(lvalue.data[i] && rvalue.data[i]))
      return false;
  }

  return true;
}

template<size_t words>
bool operator||(const BigUnsignedInteger<words> &lvalue, const BigUnsignedInteger<words> &rvalue)
{
  // || is only false if both rhs and lhs are zero
  return !(lvalue.operator==(0) && rvalue.operator==(0));
}

template<size_t words, typename T>
bool operator==(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  if (lvalue.data[0] != rvalue)
    return false;

  for (size_t i = 1; i < lvalue.len; ++i)
  {
    if (lvalue.data[i] != 0)
      return false;
  }

  return true;
}

template<size_t words, typename T>
bool operator!=(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return !operator==(lvalue, rvalue);
}

template<size_t words, typename T>
bool operator>(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  if (lvalue.data[0] > rvalue)
    return true;

  for (size_t i = 0; i < lvalue.len; ++i)
  {
    if (lvalue.data[i])
      return true;
  }

  return false;
}

template<size_t words,typename T>
bool operator<(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  if (lvalue.data[0] < rvalue)
    return true;

  return false;
}

template<size_t words, typename T>
bool operator>=(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return !operator<(lvalue, rvalue);
}

template<size_t words, typename T>
bool operator<=(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return !operator>(lvalue, rvalue);
}

template<size_t words, typename T>
bool operator&&(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return ((bool)lvalue && rvalue);
}

template<size_t words, typename T>
bool operator||(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return ((bool)lvalue || rvalue);
}

template<size_t words>
BigUnsignedInteger<words> operator+(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words>
BigUnsignedInteger<words> operator-(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words>
BigUnsignedInteger<words> operator*(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words>
BigUnsignedInteger<words> operator/(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words>
BigUnsignedInteger<words> operator%(const BigUnsignedInteger<words> &lvalue, 
  const BigUnsignedInteger<words> &rvalue)
{
  return lvalue;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator+(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return lvalue;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator-(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return lvalue;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator*(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return lvalue;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator/(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return lvalue;
}

template<size_t words, typename T>
BigUnsignedInteger<words> operator%(const BigUnsignedInteger<words> &lvalue, const T &rvalue)
{
  return lvalue;
}

#pragma endregion;

#pragma region "SignedOperators"

#pragma endregion

/** Type definitions to simulate primitive behaviour of BigIntegers.
 *
 *  The goal is to be as unobtrusive as possible, especially for platforms
 *  that do not have native uint128 and int128 support.
 *
 *  Expand as needed.
 */
typedef BigUnsignedInteger<2> __uint128_t;
typedef BigSignedInteger<2> __int128_t;

/** Standard math functions. Expand as needed.
 *
 */
inline __uint128_t pow(double b, __int128_t e)
{
  return 0;
}

#endif
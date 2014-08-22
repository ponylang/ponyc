#if !defined(PLATFORM_INT128_H) && defined(PLATFORM_IS_VISUAL_STUDIO)
#define PLATFORM_INT128_H

/** Issues to be discussed:
 *
 * 
 */

template<size_t words>
class BigUnsignedInteger
{
private:
  template<typename T>
  void assign_primitive(const T& value)
  {
    memset(&data, 0, sizeof(data));

    data[0] = value;
    len = 1;
  }

public:
  uint64_t data[words];
  size_t len; //actual length in blocks of uint64_ts.

  /** General constructor to initialize a BigUnsignedInteger
   *
   */
  BigUnsignedInteger()
  {
    // We have to clear data, because we want to simulate
    // primitive behaviour of BigIntegers.
    memset(&data, 0, sizeof(data));
    len = 0;
  }

  /** Copy constructor for BigUnsignedIntegers of the same width.
   *   
   */
  BigUnsignedInteger(const BigUnsignedInteger &x)
  {
    len = x.len;
    memcpy(&data, &x.data, sizeof(data));
  }

  /** Copy constructor for primitive built-in types of any width.
   *
   *  The problem is that this copy constructor is ambiguous and
   *  would be accepted for any T, i.e. also BigSignedInteger of
   *  any width. BigUnsignedIntegers are not a problem, as this
   *  is resolved by the copy constructor above.
   */
  template<typename T>
  BigUnsignedInteger(const T &x) : BigUnsignedInteger()
  {
    len = 1;
    data[0] = x;
  }

  /** Assignment operators to assign primitive builtin types of any width.
   * 
   */
  BigUnsignedInteger& operator=(const uint8_t value)
  {
    assign_primitive<uint8_t>(value);
    return *this;
  }

  BigUnsignedInteger& operator=(const uint16_t value)
  {
    assign_primitive<uint16_t>(value);
    return *this;
  }

  /** Resolves unwanted implicit conversions of the operator above.
   *
   *  This means that we either support assignment of BigIntegers of the 
   *  same type (and therefore sidedness) or to assign any integer that 
   *  is natively supported on the platform this code is compiled.
   */
  BigUnsignedInteger& operator=(const BigUnsignedInteger &rvalue)
  {
    memcpy(&data, &rvalue.data, sizeof(data));
    len = rvalue.len;
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
    add(this, &rvalue, this);
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
typedef enum sign_t { invalid = -2, negative = -1, zero = 0, positive = 1 } sign_t;

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
  if (b->sign == negative)
    return greater;

  return compare(a, b->magnitude);
}

template<size_t words>
static cmp_t compare(const BigSignedInteger<words>* a, const BigSignedInteger<words>* b)
{
  if (a->sign < b->sign)
    return less;
  else if (a->sign > b->sign)
    return greater;
  else
  {
    //The signs of a and b are equal.
    switch (a->sign)
    {
    case zero:
      return equal;
    case negative:
      //both a and b are negative integers, hence we can compare
      //the two magnitudes and return the opposite result:
      return cmp_t(-compare(a->magnitude, b->magnitude));
    case positive:
      //behavior is equivalent to comparing two unsigned ints
      return compare(a->magnitude, b->magnitude);
    }
  }

  //shouldn't occur
  return invalid;
}

template <size_t words>
static void add(BigUnsignedInteger<words>* a, BigUnsignedInteger<words>* b, BigUnsignedInteger<words>* res)
{
  // If either of the two operands is zero,
  // addition is equivalent to assignment
  if (a->len == 0)
  {
    res->operator=(b);
    return;
  }
  else if (b->len == 0)
  {
    res->operator=(a);
    return;
  }

  bool carry = false;
  uint64_t tmp = 0;
  size_t i = 0;

  BigUnsignedInteger<words> *longer, *shorter;
  a->len > b->len ? longer = a, shorter = b : longer = b; shorter = a;

  for (; i < longer->len; ++i)
  {
    tmp = shorter->data[i] + longer->data[i];

    // Tmp will be smaller than either of the two
    // inputs only if the addition above overflowed.
    carry = (tmp < longer->data[i]);

    if (carry)
    {
      tmp++;
      carry |= (tmp == 0);
    }

    res->data[i] = tmp;
  }

  for (; i < longer->len && carry; ++i)
  {
    tmp = longer->data[i] + 1;
    carry = (tmp == 0);
    res->data[i] = tmp;
  }

  //copy the remaining blocks of the larger number
  for (; i < longer->len; ++i)
  {
    res->data[i] = longer->data[i];
    
    if (carry)
      res->data[i] = 1;
    else
      res->len--;
  }
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
  BigUnsignedInteger<words> res;
  add(&lvalue, &rvalue, &res);
  return res;
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
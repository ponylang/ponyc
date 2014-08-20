#ifndef PLATFORM_BIGINT_H
#define PLATFORM_BIGINT_H

/** Big integer support for 64-bit platforms and compilers that do not have
 *  builtin-support for ints > 64 bits.
 */
template <size_t words, bool is_signed>
class BigInteger
{
private:
  /** Convenience constructor for primitive conversions.
   *
   */
  BigInteger(void* p, size_t bytes)
  {
    if(bytes <= sizeof(uint64_t))
      chunks = length = 1;
    else
      chunks = length = bytes/sizeof(uint64_t);

    memcpy(&this->data, p, bytes);
  }

  /** Helper functions
   *
   */
  typedef enum cmp_t { less = -1, equal = 0, greater = 1} cmp_t;
  typedef enum sign_t { negative = -1, zero = 0, positive = 1 } sign_t;

  cmp_t compare(const BigInteger &rvalue) const
  {
    cmp_t res;

    if(length < rvalue.length || sign < rvalue.sign)
      return less;
    else if(length > rvalue.length || sign > rvalue.sign)
      return greater;
    else if(sign == 0 && rvalue.sign == 0)
      return equal;
    else
    {
      size_t i = length - 1;
      
      while(i > 0)
      {
        if(data[i] == rvalue.data[i])
          continue;
        else if(data[i] > rvalue.data[i])
        {
          res = greater;
          break;
        }
        else
        {
          res = less;
          break;
        }

        i--;
      }
    }

    res = equal;

    return sign != negative ? res : cmp_t(-res);
  }

  /** Add rvalue to the receiver.
   *
   *  TODO: What if this and &rvalue are aliases of each other?
   */
  void add(const BigInteger &rvalue)
  {
    if(rvalue.length == 0)
    {
      return;
    }
    else if(this->length == 0)
    {
      operator=(rvalue);
      return;
    }

    //TODO
  }

  /** Subtract rvalue from the receiver.
   *
   */
  void sub(const BigInteger &rvalue)
  {

  }

  /** Multiply rvalue with the receiver. The result
   *  stored in the receiver.
   */
  void mult(const BigInteger &rvalue)
  {

  }

  /** Divide the receiver by rvalue. The result is stored in
   *  "quotient". The remainder is stored in the receiver.
   */
  void div(const BigInteger &rvalue, BigInteger& quotient)
  {

  }

  void and(const BigInteger &rvalue)
  {

  }

  void or(const BigInteger& rvalue)
  {

  }

  void xor(const BigInteger& rvalue)
  {

  }

  void shift(int by)
  {

  }

  void upcast(size_t new_length)
  {

  }

  bool isSigned()
  {
    return (data[length - 1] & 0x8000000000000000) != 0;
  }

public:
  size_t chunks;
  size_t length;
  sign_t sign;

  uint64_t data[words];

  BigInteger() 
  {
    /** We have to clear data, as we are attempting to
     *  simulate usage of BigIntegers as built-in primitives.
     */
    memset(&data, 0, sizeof(data));
    chunks = length = words;
    sign = zero;

    if(is_signed)
    {
      data[length - 1] |= (uint64_t)1 << 63;
    }
  };

  BigInteger(const BigInteger &b)
  {
    length = chunks = b.chunks;
    memcpy(&data, &b.data, sizeof(data));
  }

  /** Conversion constructors for primitive types.
   *
   */
  BigInteger(uint8_t)
  {

  }

  BigInteger(int8_t)
  {

  }

  BigInteger(uint16_t)
  {

  }

  BigInteger(int16_t)
  {

  }

  BigInteger(uint32_t)
  {

  }

  BigInteger(int32_t)
  {

  }

  BigInteger(uint64_t)
  {

  }

  BigInteger(int64_t)
  {

  }

  /** Conversion constructor from signed to unsigned BigIntegers
   *
   */
  BigInteger(BigInteger<words, !is_signed> &x)
  {

  }

  /** Assignment operator.
   *
   */
  BigInteger &operator=(const BigInteger<words, is_signed> &rvalue)
  {
    if(this == &rvalue)
      return *this;

    length = rvalue.length;

    /** Simulation of primitive BigInteger behaviour forces us
     *  to copy the entire rvalue, not only its relevant chunks.
     */
    memcpy(&data, &rvalue.data, sizeof(data));
    return *this;
  }

  /** Comparison operators.
   *
   */
  bool operator==(const BigInteger<words, is_signed> &rvalue)
  {
    if(length != rvalue.length)
      return false;

    for(size_t i = 0; i < length; i++)
    {
      if(data[i] != rvalue.data[i])
        return false;
    }

    return true;
  }

  bool operator!=(const BigInteger &rvalue)
  {
    return !operator==(rvalue);
  }

  bool operator<(const BigInteger &rvalue) const
  {
    return compare(rvalue) == less;
  }

  bool operator<=(const BigInteger &rvalue) const
  {
    return compare(rvalue) != greater;
  }

  bool operator>=(const BigInteger &rvalue) const
  {
    return compare(rvalue) != less;
  }

  bool operator>(const BigInteger &rvalue) const
  {
    return compare(rvalue) == greater;
  }

  /** Basic math operators.
   *
   */
  BigInteger operator+(const BigInteger &rvalue) const
  {
    BigInteger copy(*this);
    copy.add(rvalue);
    return copy;
  }

  BigInteger operator-(const BigInteger &rvalue) const
  {
    BigInteger copy(*this);
    copy.sub(rvalue);
    return copy;
  }

  BigInteger operator*(const BigInteger &rvalue) const
  {
    BigInteger copy(*this);
    copy.mult(rvalue);
    return copy;
  }

  BigInteger operator/(const BigInteger &rvalue) const
  {
    //TODO
    return *this;
  }

  BigInteger operator%(const BigInteger &rvalue) const
  {
    //TODO
    return *this;
  }

  /** Logical operators.
   *
   */
  BigInteger operator&(const BigInteger &rvalue) const
  {
    BigInteger copy(*this);
    copy.and(rvalue);
    return copy;
  }

  BigInteger operator|(const BigInteger &rvalue) const
  {
    BigInteger copy(*this);
    copy.or(rvalue);
    return copy;
  }

  BigInteger operator^(const BigInteger &rvalue) const
  {
    BigInteger copy(*this);
    copy.xor(rvalue);
    return copy;
  }

  BigInteger operator<<(int by) const
  {
    BigInteger copy(*this);
    copy.shift(-by);
    return copy;
  }

  BigInteger operator>>(int by) const
  {
    BigInteger copy(*this);
    copy.shift(by);
    return copy;
  }

  /** Assignment overloads of the above.
   *
   */
  BigInteger &operator+=(const BigInteger &rvalue)
  {
    add(rvalue);
    return *this;
  }

  BigInteger &operator-=(const BigInteger &rvalue)
  {
    sub(rvalue);
    return *this;
  }

  BigInteger &operator*=(const BigInteger &rvalue)
  {
    mult(rvalue);
    return *this;
  }

  BigInteger &operator/=(const BigInteger &rvalue)
  {
    //TODO
  }

  BigInteger &operator%=(const BigInteger &rvalue)
  {
    //TODO
  }

  BigInteger &operator&=(const BigInteger &rvalue)
  {
    and(rvalue);
    return *this;
  }

  BigInteger &operator|=(const BigInteger &rvalue)
  {
    or(rvalue);
    return *this;
  }

  BigInteger &operator^=(const BigInteger &rvalue)
  {
    xor(rvalue);
    return *this;
  }

  BigInteger &operator<<=(int by)
  {
    shift(-by);
    return *this;
  }

  BigInteger &operator>>=(int by)
  {
    shift(by);
    return *this;
  }

  /** Unary operators.
   *
   */

  //TODO

  BigInteger<words, !is_signed> operator-() const
  {
    BigInteger<words, !is_signed> n;

    for (uint8_t i = 0; i < words; i++)
    {
      n.data[i] = ~this->data[i];
    }

    return (n += 1);
  }

  /** Post- and prefix operators for increment and
   *  decrement.
   */
  BigInteger &operator++()
  {
    bool carry = true;

    for(size_t i = 0; i < length && carry; ++i)
    {
      data[i]++;
      carry = (data[i] == 0);
    }

    if(carry)
    {
      upcast(length + 1);
      length++;
      data[i] = 1;
    }

    return *this;
  }

  BigInteger &operator++(int)
  {
    BigInteger copy(*this);
    copy.operator++();
    return copy;
  }

  BigInteger &operator--()
  {
    if(length == 0 && !is_signed)
      throw "Integer overflow: Attempting to decrement unsigned zero!";

    bool carry = true;

    for(size_t i = 0; i < carry; i++)
    {
      carry = (data[i] == 0);
      data[i]--;
    }

    if(data[length - 1] == 0)
      length--;

    return *this;
  }

  BigInteger &operator--(int)
  {
    BigInteger copy(*this);
    copy.operator--();
    return copy;
  }

  /** Explicit cast operators.
   *
   */
  explicit operator size_t()
  {
    return data[0];
  }

  explicit operator double()
  {
    return (double)data[0];
  }
};

/** Typedefs to simulate primitive behaviour of integers that have a
 *  BigInteger<A,B> backend on some platforms. The goal is to be as
 *  unobtrusive as possibe.
 */
#ifdef PLATFORM_IS_VISUAL_STUDIO
inline BigInteger<2, false> pow(double, BigInteger<2, true>& int128)
{
  return 0;
}

typedef BigInteger<2, false> __uint128_t;
typedef BigInteger<2, true> __int128_t;
#endif

#endif
#if !defined(PLATFORM_BIGINT_H) && defined(PLATFORM_IS_WINDOWS)
#define PLATFORM_BIGINT_H

/** Big integer support for 64-bit platforms and compilers that do not have
 *  builtin-support for ints > 64 bits.
 *
 *  Used for: Windows x64 (MSVC/MSVC++) platforms
 */
template<uint8_t words, bool is_signed>
class BigInteger
{
private:
  uint64_t data[words];
public:
  BigInteger()
  {
    //not clearing data, as we won't read from unwritten regions
  }

  BigInteger(const BigInteger &value)
  {

  }

  template<uint8_t arg_words, bool arg_sign>
  BigInteger(const BigInteger<arg_words, arg_sign> &b)
  {

  }

  BigInteger(int value)
  {

  }

  /** Assignment operator.
   *
   */
  BigInteger &operator=(const BigInteger &rvalue)
  {
    return *this;
  }

  /** Operators for comparing BigIntegers.
   *  
   *  Supported: ==, !=, <, >, <=, >=
   */
  bool operator==(const BigInteger &rvalue) const
  {
    return false;
  }

  bool operator!=(const BigInteger &rvalue) const
  {
    return false;
  }

  bool operator<(const BigInteger &rvalue) const
  {
    return false;
  }

  bool operator>(const BigInteger &rvalue) const
  {
    return false;
  }

  bool operator<=(const BigInteger &rvalue) const
  {
    return false;
  }

  bool operator>=(const BigInteger &rvalue) const
  {
    return false;
  }

  /** Operators for basic math on BigIntegers.
   *
   *  Supported: +, +=, -, -=, *, *=, /, /=, |, |=, &, &=, ^, ^=, >>, >>=, <<,
   *             <<=
   */
  BigInteger operator+(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger &operator+=(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger operator-(const BigInteger &rvalue)
  {
    return *this;
  }
  
  BigInteger &operator-=(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger operator*(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger &operator*=(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger operator/(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger &operator/=(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger operator|(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger &operator|=(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger operator^(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger &operator^=(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger operator>>(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger &operator>>=(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger operator<<(const BigInteger &rvalue)
  {
    return *this;
  }

  BigInteger &operator<<=(const BigInteger &rvalue)
  {
    return *this;
  }

  /** Unary operators for manipulating BigIntegers.
   *
   *  Supported: !a, -a, ~a (standard 2's complement), a++, a--, ++a, --a
   */
  bool operator!() const
  {
    return false;
  }

  BigInteger<words, !is_signed> operator-() const
  {
    return *this;
  }

  BigInteger operator~() const
  {
    return *this;
  }

  BigInteger &operator++()
  {
    return *this;
  }

  BigInteger &operator--()
  {
    return *this;
  }

  BigInteger operator++(int)
  {
    return *this;
  }

  BigInteger operator--(int)
  {
    return *this;
  }

  /** Conversion operators for casts.
   *
   *  Supported: (size_t), (double), change sign.
   */
  explicit operator size_t()
  {
    return this->data[0];
  }

  explicit operator double()
  {
    return (double)this->data[0];
  }
  
  explicit operator BigInteger<words, !is_signed>()
  {

  }
};

inline BigInteger<2, false> pow(double, BigInteger<2, true>& int128)
{
  return 0;
}

typedef BigInteger<2, false> __uint128_t;
typedef BigInteger<2, true> __int128_t;

#endif
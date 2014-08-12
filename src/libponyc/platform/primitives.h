#ifndef PLATFORM_PRIMITIVES_H
#define PLATFORM_PRIMITIVES_H

#ifdef PLATFORM_IS_WINDOWS
#include <stdexcept>

class uint128_t
{
private:
  uint64_t low;
  uint64_t high;

public:
  uint128_t() : low(0), high(0) {}
  uint128_t(int value) : low(value), high(0) {} //for convenience
  uint128_t(uint64_t value) : low(value), high(0) {}
  uint128_t(const uint128_t &value) : low(value.low), high(value.high) {}

  uint128_t &operator=(const uint128_t &rvalue)
  {
    if (&rvalue != this)
    {
      low = rvalue.low;
      high = rvalue.high;
    }

    return *this;
  }

  /** Operators for comparing uint128_ts.
   *
   *  Supported: ==, !=, <, >, >=, <= 
   */
  bool operator==(const uint128_t &rvalue) const
  {
    return high == rvalue.high && low == rvalue.low;
  }

  bool operator!=(const uint128_t &rvalue) const
  {
    return high != rvalue.high && low != rvalue.low;
  }

  bool operator<(const uint128_t &rvalue) const
  {
    return (high == rvalue.high) ? low < rvalue.low : high < rvalue.high;
  }

  bool operator>(const uint128_t &rvalue) const
  {
    return (low == rvalue.low) ? high > rvalue.high : low > rvalue.low;
  }

  bool operator<=(const uint128_t &rvalue) const
  {
    return (high == rvalue.high) ? low <= rvalue.low : high <= rvalue.high;
  }

  bool operator>=(const uint128_t &rvalue) const
  {
    return (low == rvalue.low) ? high >= rvalue.high : low >= rvalue.low;
  }

  /** Operators for basic math on uint128_ts.
   *
   *  Supported: +, -, +=, -=, *, *=, /, /=, |, |=, &, &=, ^, ^=, >>, >>=, <<, 
   *             <<=
   */
  uint128_t operator+(const uint128_t &rvalue)
  {
    uint128_t copy(*this);
    return copy += rvalue;
  }

  uint128_t operator-(const uint128_t &rvalue)
  {
    uint128_t copy(*this);
    return copy -= rvalue;
  }

  uint128_t &operator+=(const uint128_t &rvalue)
  {
    uint64_t prev = low;
    
    low += rvalue.low;
    high += rvalue.high;

    //check for overflow
    if (low < prev)
    {
      ++high;
    }

    return *this;
  }

  uint128_t &operator-=(const uint128_t &rvalue)
  {
    return *this += -rvalue;
  }

  uint128_t operator*(const uint128_t &rvalue)
  {
    uint128_t copy(*this);
    return copy *= rvalue;
  }

  uint128_t &operator*=(const uint128_t &rvalue)
  {
    if (rvalue == 0)
    {
      low = 0;
      high = 0;
    }
    else if (rvalue != 1)
    {
      uint128_t self(*this);
      uint128_t tmp = rvalue;

      low = 0;
      high = 0;

      for (uint32_t i = 0; i < 128; ++i)
      {
        if ((tmp.low & 1) != 0)
        {
          *this += (self << i);
        }

        tmp >>= 1;
      }

      return *this;
    }
  }

  uint128_t operator/(const uint128_t &rvalue)
  {
    uint128_t copy(*this);
    return copy /= rvalue;
  }

  uint128_t &operator/=(const uint128_t &rvalue)
  {
    if (rvalue == 0)
    {
      throw std::runtime_error("Attempt to divide by zero!");
    }
    else if (rvalue == 1)
    {
      return *this;
    }
    else if (*this == rvalue)
    {
      low = 1;
      high = 0;
      return *this;
    }
    else if (*this == uint128_t(0) || (*this < rvalue))
    {
      low = 0;
      high = 0;
      return *this;
    }
    //TODO
  }

  uint128_t operator|(const uint128_t &rvalue)
  {
    uint128_t copy(*this);
    return copy |= rvalue;
  }

  uint128_t &operator|=(const uint128_t &rvalue)
  {
    low |= rvalue.low;
    high |= rvalue.high;
    return *this;
  }

  uint128_t operator&(const uint128_t &rvalue)
  {
    uint128_t copy(*this);
    return copy &= rvalue;
  }

  uint128_t &operator&=(const uint128_t &rvalue)
  {
    low &= rvalue.low;
    high &= rvalue.high;
    return *this;
  }

  uint128_t operator^(const uint128_t &rvalue)
  {
    uint128_t copy(*this);
    return copy ^= rvalue;
  }

  uint128_t &operator^=(const uint128_t &rvalue)
  {
    low ^= rvalue.low;
    high ^= rvalue.high;
    return *this;
  }

  uint128_t operator>>(const uint128_t &rvalue)
  {
    uint128_t copy(*this);
    return copy >>= rvalue;
  }

  uint128_t &operator>>=(const uint128_t &rvalue)
  {

  }

  uint128_t operator<<(const uint128_t& rvalue)
  {
    uint128_t copy(*this);
    return copy <<= rvalue;
  }

  uint128_t &operator<<=(const uint128_t& rvalue)
  {
    
  }
  
  /** Unary operators for manipulating uint128_ts.
   *
   *  Supported: !a, -a, ~a (standard 2's complement), a++, a--, ++a, --a 
   */
  bool operator!() const
  {
    return (high == 0 && low == 0);
  }

  uint128_t operator-() const
  {
    return ~(*this) += 1;
  }

  uint128_t operator~() const
  {
    uint128_t copy(*this);
    copy.low = ~copy.low;
    copy.high = ~copy.high;

    return copy;
  }

  uint128_t &operator++()
  {
    if (++low == 0)
    {
      ++high;
    }

    return *this;
  }

  uint128_t &operator--()
  {
    if (low-- == 0)
    {
      --high;
    }

    return *this;
  }

  /** Conversion operators for casts.
   *
   *  Casting causes data loss by truncation!
   *
   *  Supported: (size_t)
   */
  operator size_t() 
  {
    return (size_t)low;
  }
};

typedef uint128_t __uint128_t;
#endif

#endif
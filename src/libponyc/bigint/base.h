#ifndef BIGINT_BASE_H
#define BIGINT_BASE_H

#include <stdint.h>
#include <string.h>

typedef enum cmp_t 
{
  less = -1,
  equal = 0,
  greater = 1
} cmp_t;

typedef enum sign_t
{
  negative = -1,
  zero = 0,
  positive = 1
} sign_t;

template<size_t words>
class IntArray
{
public:
  //should be private
  size_t len;
  uint64_t data[words];

  IntArray()
  {
    len = 0;
    memset(&data, 0, sizeof(data));
  }

  IntArray(const IntArray& val)
  {
    len = val.len;
    memcpy(&data, &val.data, sizeof(data));
  }

  void operator=(const IntArray& val)
  {
    if (this == &val)
      return;

    len = val.len;

    for (size_t i = 0; i < len; ++i)
      data[i] = val.data[i];
  }

  bool operator==(const IntArray& val) const
  {
    for (size_t i = 0; i < len; ++i)
    {
      if (data[i] != val.data[i])
        return false;
    }

    return true;
  }

  bool operator!=(const IntArray& val) const
  {
    return !operator==(val);
  }
};

#endif
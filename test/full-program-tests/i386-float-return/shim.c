#if defined(__i386__) || defined(_M_IX86)
#include <stdbool.h>
#include <stdint.h>

uint32_t i386_float_bits32(void)
{
  return UINT32_C(0x7f800001);
}

uint64_t i386_float_bits64(void)
{
  return UINT64_C(0x7ff0000000000001);
}

bool i386_call_f32(void* receiver, float (*fn)(void*))
{
  return fn(receiver) == 3.5f;
}

bool i386_call_f64(void* receiver, double (*fn)(void*), double expected)
{
  return fn(receiver) == expected;
}

bool i386_call_bare(float (*fn)(void))
{
  return fn() == 1.25f;
}
#endif

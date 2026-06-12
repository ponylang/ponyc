#include <stdint.h>
#include <string.h>
#include "shim_extra.h"

#ifndef SHIM_BASE
#define SHIM_BASE 0
#endif

int32_t shim_answer(void)
{
  /* strlen comes from the target's libc headers, not clang's builtin
   * (freestanding-safe) headers, so this test fails loudly if the
   * hand-rolled system include discovery breaks. "0123456789" gives 10;
   * SHIM_BASE (30, via cdefine:) + SHIM_EXTRA (2, via cinclude:) + 10 = 42.
   */
  return SHIM_BASE + SHIM_EXTRA + (int32_t)strlen("0123456789");
}

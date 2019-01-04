#ifndef PLATFORM_PONYASSERT_H
#define PLATFORM_PONYASSERT_H

#include "platform.h"

PONY_EXTERN_C_BEGIN

#if !defined(PONY_NDEBUG) && !defined(PONY_ALWAYS_ASSERT) && defined(NDEBUG)
#  define PONY_NDEBUG
#endif

#if defined(PONY_NDEBUG)
#  define pony_assert(expr) ((void)0)
#else
#  define pony_assert(expr) \
    ((expr) ? (void)0 : \
      ponyint_assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif

void ponyint_assert_fail(const char* expr, const char* file, size_t line,
  const char* func);

void ponyint_assert_disable_popups();

PONY_EXTERN_C_END

#endif

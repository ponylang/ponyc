#ifndef PLATFORM_ATOMICS_H
#define PLATFORM_ATOMICS_H

/** Intrinsics for atomic memory operations on x86 machines.
 *
 *  6 different, platform independent memory modes are supported, sorted by
 *  strength ascending:
 *
 *    RELAXED
 *      Does not use any synchronisation or memory barriers, but forces the
 *      the compiler to not re-order memory operations.
 *    CONSUME
 *      Data dependency for both barriers and synchronisation with another
 *      thread.
 *    ACQUIRE
 *      Barrier to synchronize code of stores with semantic RELEASE
 *      (or stronger) of another thread.
 *    RELEASE
 *      Barrier to synchronize code of loads with semantic ACQUIRE
 *      (or stronger) of another thread.
 *    ACQ_REL
 *      Full barrier in both directions for acquire loads and release stores in
 *      another thread.
 *    SEQ_CST
 *      Full barrier in both directions for acquire loads and release stores in
 *      all threads.
 *
 *    On Windows, InterlockedCompareExchange128 is always applying a full
 *    fence. Also, any atomic operation on Windows is strong, i.e. cannot fail
 *    spuriously.
 *
 *    Docs:
 *      https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
 *      http://msdn.microsoft.com/en-us/library/12a04hfd.aspx?ppud=4
 *      http://msdn.microsoft.com/en-us/library/windows/desktop/
 *      ms686360%28v=vs.85%29.aspx
 */
#if defined(PLATFORM_IS_CLANG_OR_GCC)

  #define _atomic_load(PTR, MODE) \
    (__atomic_load_n(PTR, MODE))

  #define _atomic_store(PTR, VAL, MODE) \
    (__atomic_store_n(PTR, VAL, MODE))

  #define _atomic_exch(PTR, VAL, MODE) \
    (__atomic_exchange_n(PTR, VAL, MODE))

  #define _atomic_cas(PTR, EXPP, VAL, OK, FAIL) \
    (__atomic_compare_exchange_n(PTR, EXPP, VAL, false, OK, FAIL))

  #define _atomic_dwcas(PTR, EXPP, VAL, OK, FAIL) \
    (__atomic_compare_exchange_n(PTR, EXPP, VAL, false, OK, FAIL))

  #define _atomic_add32(PTR, VAL, MODE) \
    (__atomic_add_fetch(PTR, VAL, MODE))

  #define _atomic_sub32(PTR, VAL, MODE) \
    (__atomic_sub_fetch(PTR, VAL, MODE))

  #define _atomic_add64(PTR, VAL, MODE) \
    (__atomic_add_fetch(PTR, VAL, MODE))

  #define _atomic_sub64(PTR, VAL, MODE) \
    (__atomic_sub_fetch(PTR, VAL, MODE))

#elif defined(PLATFORM_IS_VISUAL_STUDIO)

  #pragma intrinsic(_InterlockedExchangePointer)
  #pragma intrinsic(_InterlockedCompareExchangePointer)
  #pragma intrinsic(_InterlockedCompareExchange128)

  #define _atomic_load(PTR, MODE) \
    (*(PTR))

  #define _atomic_store(PTR, VAL, MODE) \
    (*(PTR) = (VAL))

  #define _atomic_exch(PTR, VAL, MODE) \
    (_InterlockedExchangePointer((volatile PVOID*)PTR, VAL))

  #define _atomic_cas(PTR, EXPP, VAL, OK, FAIL) \
    (_InterlockedCompareExchangePointer((PVOID volatile*)PTR, VAL, *(EXPP)) \
      == *(EXPP))

  #define _atomic_dwcas(PTR, EXPP, VAL, OK, FAIL) \
    (_InterlockedCompareExchange128((LONGLONG volatile*)PTR, VAL.high, \
      VAL.low, (LONGLONG*)EXPP))

  #define _atomic_add32(PTR, VAL, MODE) \
    (InterlockedAddNoFence((LONG volatile*)PTR, VAL))

  #define _atomic_sub32(PTR, VAL, MODE) \
    (InterlockedAddNoFence((LONG volatile*)PTR, -(VAL)))

  #define _atomic_add64(PTR, VAL, MODE) \
    (InterlockedAdd64((LONGLONG volatile*)PTR, VAL))

  #define _atomic_sub64(PTR, VAL, MODE) \
    (InterlockedAdd64((LONGLONG volatile*)PTR, -(VAL)))

#endif

#endif

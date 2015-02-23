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
 *      Barrier to synchronize code of stores with semantic PONY_ATOMIC_RELEASE
 *      (or stronger) of another thread.
 *    RELEASE
 *      Barrier to synchronize code of loads with semantic PONY_ATOMIC_ACQUIRE
 *      (or stronger) of another thread.
 *    ACQ_REL
 *      Full barrier in both directions for acquire loads and release stores in
 *      another thread.
 *    SEQ_CST
 *      Full barrier in both directions for acquire loads and release stores in
 *      all threads.
 *
 *    atomic_nand is not supported.
 *    atomic_[op]_fetch* are not supported.
 *    atomic_test_and_set is not supported.
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
#ifdef PLATFORM_IS_CLANG_OR_GCC

#  define PONY_ATOMIC_RELAXED __ATOMIC_RELAXED
#  define PONY_ATOMIC_CONSUME __ATOMIC_CONSUME
#  define PONY_ATOMIC_ACQUIRE __ATOMIC_ACQUIRE
#  define PONY_ATOMIC_RELEASE __ATOMIC_RELEASE
#  define PONY_ATOMIC_ACQ_REL __ATOMIC_ACQ_REL
#  define PONY_ATOMIC_SEQ_CST __ATOMIC_SEQ_CST

#  define __pony_atomic_load_n(PTR, MODE, T) \
            __atomic_load_n(PTR, MODE)
#  define __pony_atomic_load(PTR, RETP, MODE, T) \
            __atomic_load(PTR, RETP MODE)
#  define __pony_atomic_store_n(PTR, VAL, MODE, T) \
            __atomic_store_n(PTR, VAL, MODE)
#  define __pony_atomic_store(PTR, VALP, MODE, T) \
            __atomic_store(PTR, VALP, MODE)
#  define __pony_atomic_exchange_n(PTR, VAL, MODE, T) \
            __atomic_exchange_n(PTR, VAL, MODE)
#  define __pony_atomic_exchange(PTR, VALP, RETP, MODE, T) \
            __atomic_exchange(PTR, VALP, RETP)
#  define __pony_atomic_compare_exchange_n(PTR, EXPP, VAL, WEAK, OK, FAIL, T) \
            __atomic_compare_exchange_n(PTR, EXPP, VAL, WEAK, OK, FAIL)
#  define __pony_atomic_compare_exchange(PTR, EXPP, VALP, WEAK, OK, FAIL, T) \
            __atomic_compare_exchange(PTR, EXPP, VALP, WEAK, OK, FAIL)
#  define __pony_atomic_fetch_add(PTR, VAL, MODE, T) \
            __atomic_fetch_add(PTR, VAL, MODE)
#  define __pony_atomic_fetch_sub(PTR, VAL, MODE, T) \
            __atomic_fetch_sub(PTR, VAL, MODE)
#  define __pony_atomic_fetch_and(PTR, VAL, MODE, T) \
            __atomic_fetch_and(PTR, VAL, MODE)
#  define __pony_atomic_fetch_xor(PTR, VAL, MODE, T) \
            __atomic_fetch_xor(PTR, VAL, MODE)
#  define __pony_atomic_fetch_or(PTR, VAL, MODE, T) \
            __atomic_fetch_or(PTR, VAL, MODE)

#elif defined(PLATFORM_IS_VISUAL_STUDIO)

#pragma intrinsic(_InterlockedExchangePointer)
#pragma intrinsic(_InterlockedCompareExchangePointer)
#pragma intrinsic(_InterlockedCompareExchange128)

#define PONY_ATOMIC_RELAXED relaxed
#define PONY_ATOMIC_CONSUME consume
#define PONY_ATOMIC_ACQUIRE acquire
#define PONY_ATOMIC_RELEASE release
#define PONY_ATOMIC_ACQ_REL acq_rel
#define PONY_ATOMIC_SEQ_CST seq_cst

#define PONY_ATOMIC_NO_TYPE none

#  define __INTERLOCKED(X, Y, Z) X ## _ ## Y ## _ ## Z

#  define __pony_atomic_load_n(PTR, MODE, T) \
            __INTERLOCKED(__pony_atomic_load_n, MODE, T)(PTR)
#  define __pony_atomic_load(PTR, RETP, MODE, T) \
            __INTERLOCKED(__pony_atomic_load, MODE, T)(PTR, RETP)
#  define __pony_atomic_store_n(PTR, VAL, MODE, T) \
            __INTERLOCKED(__pony_atomic_store_n, MODE, T)(PTR, VAL)
#  define __pony_atomic_store(PTR, VALP, MODE, T) \
            __INTERLOCKED(__pony_atomic_store, MODE, T)(PTR, VALP)
#  define __pony_atomic_exchange_n(PTR, VAL, MODE, T) \
            __INTERLOCKED(__pony_atomic_exchange_n, MODE, T)(PTR, VAL)
#  define __pony_atomic_exchange(PTR, VALP, RETP, MODE, T) \
            __INTERLOCKED(__pony_atomic_exchange, MODE, T)(PTR, VALP, RETP)
#  define __pony_atomic_compare_exchange_n(PTR, EXPP, VAL, WEAK, OK, FAIL, T) \
            __INTERLOCKED(__pony_atomic_compare_exchange_n, OK, T)(PTR, EXPP, \
              VAL)
#  define __pony_atomic_compare_exchange(PTR, EXPP, VALP, WEAK, OK, FAIL, T) \
            __INTERLOCKED(__pony_atomic_compare_exchange, OK, T)(PTR, EXPP, \
              VALP)
#  define __pony_atomic_fetch_add(PTR, VAL, MODE, T) \
            __INTERLOCKED(__pony_atomic_fetch_add, MODE, T)(PTR, VAL)
#  define __pony_atomic_fetch_sub(PTR, VAL, MODE, T) \
            __INTERLOCKED(__pony_atomic_fetch_sub, MODE, T)(PTR, VAL)
#  define __pony_atomic_fetch_and(PTR, VAL, MODE, T) \
            __INTERLOCKED(__pony_atomic_fetch_and, MODE, T)(PTR, VAL)
#  define __pony_atomic_fetch_xor(PTR, VAL, MODE, T) \
            __INTERLOCKED(__pony_atomic_fetch_xor, MODE, T)(PTR, VAL)
#  define __pony_atomic_fetch_or(PTR, VAL, MODE, T) \
            __INTERLOCKED(__pony_atomic_fetch_or, MODE, T)(PTR, VAL)

#  define __pony_atomic_load_n_relaxed_none(PTR) \
            *(PTR)
#  define __pony_atomic_load_n_acquire_none(PTR) \
            *(PTR)
#  define __pony_atomic_store_n_relaxed_none(PTR, VAL) \
            *(PTR) = VAL
#  define __pony_atomic_store_n_release_none(PTR, VAL) \
            *(PTR) = VAL
#  define __pony_atomic_load_n_acquire_uint64_t(PTR) \
            *(PTR)
#  define __pony_atomic_fetch_add_relaxed_uint32_t(PTR, VAL) \
            InterlockedAddNoFence((LONG volatile*)PTR, VAL)
#  define __pony_atomic_fetch_sub_relaxed_uint32_t(PTR, VAL) \
            InterlockedAddNoFence((LONG volatile*)PTR, -(VAL))
#  define __pony_atomic_fetch_add_release_uint64_t(PTR, VAL) \
            InterlockedAdd64((LONGLONG volatile*)PTR, VAL)
#  define __pony_atomic_fetch_sub_release_uint64_t(PTR, VAL) \
            InterlockedAdd64((LONGLONG volatile*)PTR, -(VAL))
#  define __pony_atomic_exchange_n_relaxed_intptr_t(PTR, VAL) \
            _InterlockedExchangePointer((volatile PVOID*)PTR, VAL)
#  define __pony_atomic_compare_exchange_n_relaxed_intptr_t(PTR, EXPP, VAL) \
            (_InterlockedCompareExchangePointer((PVOID volatile*)PTR, \
              VAL, *(EXPP)) == *(EXPP))
#  define __pony_atomic_compare_exchange_n_relaxed___int128_t(PTR, EXPP, VAL) \
            _InterlockedCompareExchange128((LONGLONG volatile*)PTR, VAL.high, \
              VAL.low, (LONGLONG*)EXPP)
#  define __pony_atomic_compare_exchange_n_acq_rel___int128_t(PTR, EXPP, VAL) \
            _InterlockedCompareExchange128((LONGLONG volatile*)PTR, VAL.high, \
              VAL.low, (LONGLONG*)EXPP)
#endif

#endif

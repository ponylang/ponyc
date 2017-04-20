#ifndef PONY_DETAIL_ATOMICS_H
#define PONY_DETAIL_ATOMICS_H

#if !defined(__ARM_ARCH_2__) && !defined(__arm__) && !defined(__aarch64__) && \
 !defined(__i386__) && !defined(_M_IX86) && !defined(_X86_) && \
 !defined(__amd64__) && !defined(__x86_64__) && !defined(_M_X64) && \
 !defined(_M_AMD64)
#  error "Unsupported platform"
#endif

#ifdef _MSC_VER
// MSVC has no support of C11 atomics.
#  include <atomic>
#  define PONY_ATOMIC(T) std::atomic<T>
#  ifdef PONY_WANT_ATOMIC_DEFS
using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

using std::atomic_load_explicit;
using std::atomic_store_explicit;
using std::atomic_exchange_explicit;
using std::atomic_compare_exchange_weak_explicit;
using std::atomic_compare_exchange_strong_explicit;
using std::atomic_fetch_add_explicit;
using std::atomic_fetch_sub_explicit;
using std::atomic_thread_fence;
#  endif
#elif defined(__GNUC__) && !defined(__clang__)
#  include <features.h>
#  if __GNUC_PREREQ(4, 9)
#    ifdef __cplusplus
//     g++ doesn't like C11 atomics. We never need atomic ops in C++ files so
//     we only define the atomic types.
#      include <atomic>
#      define PONY_ATOMIC(T) std::atomic<T>
#    else
#      ifdef PONY_WANT_ATOMIC_DEFS
#        include <stdatomic.h>
#      endif
#      define PONY_ATOMIC(T) T _Atomic
#    endif
#  elif __GNUC_PREREQ(4, 7)
// Valid on x86 and ARM given our uses of atomics.
#    define PONY_ATOMIC(T) T
#    define PONY_ATOMIC_BUILTINS
#  else
#    error "Please use GCC >= 4.7"
#  endif
#elif defined(__clang__)
#  if __clang_major__ >= 4 || (__clang_major__ == 3 && __clang_minor__ >= 6)
#    ifdef PONY_WANT_ATOMIC_DEFS
#      include <stdatomic.h>
#    endif
#    define PONY_ATOMIC(T) T _Atomic
#  elif __clang_major__ >= 3 && __clang_minor__ >= 3
// Valid on x86 and ARM given our uses of atomics.
#    define PONY_ATOMIC(T) T
#    define PONY_ATOMIC_BUILTINS
#  else
#    error "Please use Clang >= 3.3"
#  endif
#else
#  error "Unsupported compiler"
#endif

#ifdef _MSC_VER
namespace ponyint_atomics
{
  template <typename T>
  struct aba_protected_ptr_t
  {
    // Nested struct for uniform initialisation with GCC/Clang.
    struct
    {
      T* object;
      uintptr_t counter;
    };
  };
}
#  define PONY_ABA_PROTECTED_PTR_DECLARE(T)
#  define PONY_ABA_PROTECTED_PTR(T) ponyint_atomics::aba_protected_ptr_t<T>
#else
#  if defined(__LP64__) || defined(_WIN64)
#    define PONY_DOUBLEWORD __int128_t
#  else
#    define PONY_DOUBLEWORD int64_t
#  endif
#  define PONY_ABA_PROTECTED_PTR_DECLARE(T) \
    typedef union \
    { \
      struct \
      { \
        T* object; \
        uintptr_t counter; \
      }; \
      PONY_DOUBLEWORD raw; \
    } aba_protected_##T;
#  define PONY_ABA_PROTECTED_PTR(T) aba_protected_##T
#endif

// Big atomic objects (larger than machine word size) aren't consistently
// implemented on the compilers we support. We add our own implementation to
// make sure the objects are correctly defined and aligned.
#define PONY_ATOMIC_ABA_PROTECTED_PTR(T) alignas(16) PONY_ABA_PROTECTED_PTR(T)

#ifdef PONY_WANT_ATOMIC_DEFS
#  ifdef _MSC_VER
#    pragma warning(disable:4164)
#    pragma intrinsic(_InterlockedCompareExchange128)

namespace ponyint_atomics
{
  template <typename T>
  inline PONY_ABA_PROTECTED_PTR(T) big_load(PONY_ABA_PROTECTED_PTR(T)* ptr)
  {
    PONY_ABA_PROTECTED_PTR(T) ret = {NULL, 0};
    _InterlockedCompareExchange128((LONGLONG*)ptr, 0, 0, (LONGLONG*)&ret);
    return ret;
  }

  template <typename T>
  inline void big_store(PONY_ABA_PROTECTED_PTR(T)* ptr,
    PONY_ABA_PROTECTED_PTR(T) val)
  {
    PONY_ABA_PROTECTED_PTR(T) tmp;
    tmp.object = ptr->object;
    tmp.counter = ptr->counter;
    while(!_InterlockedCompareExchange128((LONGLONG*)ptr,
      (LONGLONG)val.counter, (LONGLONG)val.object, (LONGLONG*)&tmp))
    {}
  }
}

#    define bigatomic_load_explicit(PTR, MO) \
      ponyint_atomics::big_load(PTR)

#    define bigatomic_store_explicit(PTR, VAL, MO) \
      ponyint_atomics::big_store(PTR, VAL)

#    define bigatomic_compare_exchange_weak_explicit(PTR, EXP, DES, SUCC, FAIL) \
      _InterlockedCompareExchange128((LONGLONG*)PTR, (LONGLONG)((DES).counter), \
        (LONGLONG)((DES).object), (LONGLONG*)EXP)

#    pragma warning(default:4164)
#  else
#    define bigatomic_load_explicit(PTR, MO) \
      (__typeof__(*(PTR)))__atomic_load_n(&(PTR)->raw, MO)

#    define bigatomic_store_explicit(PTR, VAL, MO) \
      __atomic_store_n(&(PTR)->raw, (VAL).raw, MO)

#    define bigatomic_compare_exchange_weak_explicit(PTR, EXP, DES, SUCC, FAIL) \
      __atomic_compare_exchange_n(&(PTR)->raw, &(EXP)->raw, (DES).raw, true, \
        SUCC, FAIL)
#  endif

#  ifdef PONY_ATOMIC_BUILTINS
#    define memory_order_relaxed __ATOMIC_RELAXED
#    define memory_order_consume __ATOMIC_CONSUME
#    define memory_order_acquire __ATOMIC_ACQUIRE
#    define memory_order_release __ATOMIC_RELEASE
#    define memory_order_acq_rel __ATOMIC_ACQ_REL
#    define memory_order_seq_cst __ATOMIC_SEQ_CST

#    define atomic_load_explicit(PTR, MO) \
      ({ \
        _Static_assert(sizeof(PTR) <= sizeof(void*), ""); \
        __atomic_load_n(PTR, MO); \
      })

#    define atomic_store_explicit(PTR, VAL, MO) \
      ({ \
        _Static_assert(sizeof(PTR) <= sizeof(void*), ""); \
        __atomic_store_n(PTR, VAL, MO); \
      })

#    define atomic_exchange_explicit(PTR, VAL, MO) \
      ({ \
        _Static_assert(sizeof(PTR) <= sizeof(void*), ""); \
        __atomic_exchange_n(PTR, VAL, MO); \
      })

#    define atomic_compare_exchange_weak_explicit(PTR, EXP, DES, SUCC, FAIL) \
      ({ \
        _Static_assert(sizeof(PTR) <= sizeof(void*), ""); \
        __atomic_compare_exchange_n(PTR, EXP, DES, true, SUCC, FAIL); \
      })

#    define atomic_compare_exchange_strong_explicit(PTR, EXP, DES, SUCC, FAIL) \
      ({ \
        _Static_assert(sizeof(PTR) <= sizeof(void*), ""); \
        __atomic_compare_exchange_n(PTR, EXP, DES, false, SUCC, FAIL); \
      })

#    define atomic_fetch_add_explicit(PTR, VAL, MO) \
      ({ \
        _Static_assert(sizeof(PTR) <= sizeof(void*), ""); \
        __atomic_fetch_add(PTR, VAL, MO); \
      })

#    define atomic_fetch_sub_explicit(PTR, VAL, MO) \
      ({ \
        _Static_assert(sizeof(PTR) <= sizeof(void*), ""); \
        __atomic_fetch_sub(PTR, VAL, MO); \
      })

#    define atomic_thread_fence(MO) \
      __atomic_thread_fence(MO)

#    undef PONY_ATOMIC_BUILTINS
#  endif
#endif

#endif

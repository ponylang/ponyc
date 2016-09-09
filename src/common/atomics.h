#ifndef PLATFORM_ATOMICS_H
#define PLATFORM_ATOMICS_H

// C and C++ atomics are incompatible and MSVC has no support of C atomics.
// This header provides a workaround.

#ifdef __cplusplus
#  include <atomic>
#  define ATOMIC_TYPE(T) std::atomic<T>
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
#else
#  include <stdatomic.h>
#  define ATOMIC_TYPE(T) T _Atomic
#endif

#endif

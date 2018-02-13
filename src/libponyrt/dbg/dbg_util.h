#ifndef DBG_UTIL_H
#define DBG_UTIL_H

// Assume #define z 123
// Assume #define FOO z

// xstr(FOO) -> "123"
#define dbg_xstr(z) dbg_str(z)

// str(FOO)  -> "FOO"
#define dbg_str(s) #s

// Useful to mark unused parameters to routines
#if !defined(MAYBE_UNUSED)
#define MAYBE_UNUSED(x) (void)x
#endif

#if !defined(UNUSED)
#define UNUSED(x) (void)x
#endif

/**
 * Define INITIALIZER and FINALIZER macros
 * Example:
 *
 * // Initialize debug context
 * static dbg_ctx_t* dc = NULL;
 * INITIALIZER(initialize)
 * {
 *   dc = dc_create(stderr, 32);
 *   fprintf(stderr, "initialize:# %s\n", __FILE__);
 * }
 *
 * //  Finalize debug context
 * FINALIZER(finalize)
 * {
 *   dc_destroy(dc);
 *   dc = NULL;
 *   fprintf(stderr, "finalize:# %s\n", __FILE__);
 * }
 */
//#ifdef _MSC_VER
#if 0
// Disable for MSC as we get an error something like:
// "waring LNK4078 multiple sections are defined with different attributes"

#define INITIALIZER(f) \
  static void f(); \
  static int __i1(){f();return 0;} \
  __pragma(data_seg(".CRT$XIU")) \
  static int(*__i2) () = __i1; \
  __pragma(data_seg()) \
  static void f()

#define FINALIZER(f) \
  static void f(); \
  static int __f1(){f();return 0;} \
  __pragma(data_seg(".CRT$XPU")) \
  static int(*__f2) () = __f1; \
  __pragma(data_seg()) \
  static void f()

#else

#define INITIALIZER(f) \
  static void __attribute__ ((constructor)) f()

#define FINALIZER(f) \
  static void __attribute__ ((destructor)) f()

#endif

#define _DBG_BIT_IDX_NAME(base_name) (base_name ## _bit_idx)
#define _DBG_BIT_CNT_NAME(base_name) (base_name ## _bit_cnt)

#define _DBG_NEXT_IDX(base_name) (_DBG_BIT_IDX_NAME(base_name) + \
                                    _DBG_BIT_CNT_NAME(base_name))

#define DBG_BITS_FIRST(name, cnt) \
  name ## _bit_idx = 0, name ## _bit_cnt = cnt

#define DBG_BITS_NEXT(name, cnt, previous) \
  name ## _bit_idx = _DBG_NEXT_IDX(previous), name ## _bit_cnt = cnt

#define DBG_BITS_SIZE(name, previous) \
  name = _DBG_NEXT_IDX(previous)

/**
 * Convert a base_name and offset to an index into
 * the bits array.
 *
 * Example: Bit regions can be defined with a base_name
 * and size and placed in an enum.  First below define
 * an enum with three bit resions with the names, first,
 * second and another. And the the last enum is the size
 * of the necessary bits array to hold these bit regions.
 *
 * enum {
 *   DBG_BITS_FIRST(first, 30),
 *   DBG_BITS_NEXT(second, 3, first),
 *   DBG_BITS_NEXT(another, 2, second),
 *   DBG_BITS_SIZE(bits_size, another)
 * };
 *
 *
 * Then the macro dbg_bnoi can be use to convert the
 * base_name and an offset to an index into the bits array.
 *
 * Below is some code. We create debug context, dc, with
 * bit_size used to allocate the bits array.  Next we use
 * dbg_sb and dbg_bnoi to set the last bit, bit 2, of the
 * second region. Followed with using dbg_pf and dbg_bnoi
 * to print "Hello, World".  Finally, we destroy the debug
 * context.
 *
 * dbg_ctx_t* dc = dbg_ctx_create_with_dst_file(number_of_bits, stderr);
 * dbg_sb(dc, dbg_bnoi(second, 2), 1);
 * dbg_pf(dc, dbg_bnoi(second, 2), "Hello, %s\n", "World");
 * dbg_ctx_destroy(dc);
 */
#define dbg_bnoi(base_name, offset) \
  (_DBG_BIT_IDX_NAME(base_name) + offset)

#endif

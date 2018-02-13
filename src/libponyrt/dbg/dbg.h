#ifndef DBG_H
#define DBG_H

#include <platform.h>
#include <stdio.h>
#include <stdint.h>

PONY_EXTERN_C_BEGIN

#if !defined(DBG_ENABLED)
#define DBG_ENABLED false
#endif

#define _DBG_DO(...) do {__VA_ARGS__;} while(0)

#define _DBG_BITS_ARRAY_IDX(bit_idx) ((bit_idx) / 32)
#define _DBG_BIT_MASK(bit_idx) ((size_t)1 << (size_t)((bit_idx) & 0x1F))

/**
 * Debug context structure.
 *
 * I've decided that the more important information of any single
 * "print/write" operation to the is the beginning information. As
 * such when writing to dst_buf only the first N bytes will be
 * preserved. Where N is the min(dst_buf_size, tmp_buf_size).
 */
typedef struct {
  size_t* bits;
  FILE* dst_file;
  uint8_t* dst_buf;
  size_t dst_buf_size;
  size_t dst_buf_begi;
  size_t dst_buf_endi;
  size_t dst_buf_cnt;
  uint8_t* tmp_buf;
  size_t tmp_buf_size;
  size_t max_size;
} dbg_ctx_t;

/**
 * Create a dbg_ctx no char buf or file
 */
dbg_ctx_t* dbg_ctx_create(size_t number_of_bits);

/**
 * Create a dbg_ctx with char buf as destination
 */
dbg_ctx_t* dbg_ctx_create_with_dst_buf(size_t number_of_bits, size_t dst_size,
  size_t tmp_buf_size);

/**
 * Create a dbg_ctx with a FILE as as destination
 */
dbg_ctx_t* dbg_ctx_create_with_dst_file(size_t number_of_bits, FILE* dst);

/**
 * Destroy a previously created dbg_ctx
 */
void dbg_ctx_destroy(dbg_ctx_t* dc);

/**
 * Print a formated string to the current dbg_ctx destination
 */
size_t dbg_printf(dbg_ctx_t* dc, const char* format, ...);

/**
 * Print a formated string to the current dbg_ctx destination with
 * the args being a va_list.
 */
size_t dbg_vprintf(dbg_ctx_t* dc, const char* format, va_list vlist);

/**
 * Read data from the dc to dst. dst_size is size of
 * the dst and must be > size to accommodate the null
 * terminator written at the end. Size is the maximum size
 * to read.
 *
 * @returns number of bytes read not including the null
 * terminator written at the end.
 */
size_t dbg_read(dbg_ctx_t* dc, char* dst, size_t dst_size, size_t size);

/**
 * Set bit at bit_idx to bit_value
 */
static inline void dbg_sb(dbg_ctx_t* dc, size_t bit_idx, bool bit_value)
{
  if(dc != NULL)
  {
    size_t bits_array_idx = _DBG_BITS_ARRAY_IDX(bit_idx);
    size_t bit_mask = _DBG_BIT_MASK(bit_idx);
    if(bit_value)
    {
      dc->bits[bits_array_idx] |= bit_mask;
    } else {
      dc->bits[bits_array_idx] &= ~bit_mask;
    }
  }
}

/**
 * Get bit at bit_idx
 */
static inline bool dbg_gb(dbg_ctx_t* dc, size_t bit_idx)
{
  if(dc != NULL)
  {
    size_t bits_array_idx = _DBG_BITS_ARRAY_IDX(bit_idx);
    return (dc->bits[bits_array_idx] & _DBG_BIT_MASK(bit_idx)) != 0;
  } else {
    return false;
  }
}

/**
 * Unconditionally print, ignores dc->bits
 */
#define DBG_PFU(dc, format, ...) \
  _DBG_DO(dbg_printf(dc, format, __VA_ARGS__))

/**
 * Unconditionally print a string, ignores dc->bits
 * Needed for use on Windows
 */
#define DBG_PSU(dc, str) \
  _DBG_DO(dbg_printf(dc, str))

/**
 * Unconditionally print with function name, ignores dc->bits
 */
#define DBG_PFNU(dc, format, ...) \
  _DBG_DO(dbg_printf(dc, "%s:  " format, __FUNCTION__, __VA_ARGS__))

/**
 * Unconditionally print string with function name, ignores dc->bits
 */
#define DBG_PSNU(dc, str) \
  _DBG_DO(dbg_printf(dc, "%s:  " str, __FUNCTION__))

/**
 * Printf if bit_idx is set
 */
#define DBG_PF(dc, bit_idx, format, ...) \
  _DBG_DO( \
    if(DBG_ENABLED) \
      if(dbg_gb(dc, bit_idx)) \
        dbg_printf(dc, format, __VA_ARGS__))

/**
 * Print string if bit_idx is set
 */
#define DBG_PS(dc, bit_idx, str) \
  _DBG_DO( \
    if(DBG_ENABLED) \
      if(dbg_gb(dc, bit_idx)) \
        dbg_printf(dc, str))

/**
 * Printf with leading "<funcName>:  " if bit_idx is set
 */
#define DBG_PFN(dc, bit_idx, format, ...) \
  _DBG_DO( \
    if(DBG_ENABLED) \
      if(dbg_gb(dc, bit_idx)) \
        dbg_printf(dc, "%s:  " format, __FUNCTION__, __VA_ARGS__))

/**
 * Print string with leading "<funcName>:  " if bit_idx is set
 */
#define DBG_PSN(dc, bit_idx, str) \
  _DBG_DO( \
    if(DBG_ENABLED) \
      if(dbg_gb(dc, bit_idx)) \
        dbg_printf(dc, "%s:  " str, __FUNCTION__))

/**
 * "Enter routine" prints "<funcName>:+\n" if bit_idx is set
 */
#define DBG_E(dc, bit_idx) \
  _DBG_DO( \
    if(DBG_ENABLED) \
      if(dbg_gb(dc, bit_idx)) \
        dbg_printf(dc, "%s:+\n", __FUNCTION__))

/**
 * Enter routine print leading functionName:+ and new line
 * if bit_idx is set
 */
#define DBG_PFE(dc, bit_idx, format, ...) \
  _DBG_DO( \
     if(DBG_ENABLED) \
       if(dbg_gb(dc, bit_idx)) \
         dbg_printf(dc, "%s:+ " format, __FUNCTION__, __VA_ARGS__))

/**
 * Enter routine print string with leading functionName:+ and new line
 * if bit_idx is set
 */
#define DBG_PSE(dc, bit_idx, str) \
  _DBG_DO( \
     if(DBG_ENABLED) \
       if(dbg_gb(dc, bit_idx)) \
         dbg_printf(dc, "%s:+ " str, __FUNCTION__))

/**
 * "Exit routine" prints "<funcName>:-\n" if bit_idx is set
 */
#define DBG_X(dc, bit_idx) \
  _DBG_DO( \
    if(DBG_ENABLED) \
      if(dbg_gb(dc, bit_idx)) \
        dbg_printf(dc, "%s:-\n", __FUNCTION__))

/**
 * Exit routine print leading functionName:+ and new line
 * if bit_idx is set
 */
#define DBG_PFX(dc, bit_idx, format, ...) \
  _DBG_DO( \
     if(DBG_ENABLED) \
       if(dbg_gb(dc, bit_idx)) \
         dbg_printf(dc, "%s:- " format, __FUNCTION__, __VA_ARGS__))

/**
 * Exit routine print string with leading functionName:+ and new line
 * if bit_idx is set
 */
#define DBG_PSX(dc, bit_idx, str) \
  _DBG_DO( \
     if(DBG_ENABLED) \
       if(dbg_gb(dc, bit_idx)) \
         dbg_printf(dc, "%s:- " str, __FUNCTION__))

/**
 * Set bit
 */
void dbg_set_bit(dbg_ctx_t* dc, size_t bit_idx, bool bit_value);

/**
 * Get bit
 */
bool dbg_get_bit(dbg_ctx_t* dc, size_t bit_idx);

PONY_EXTERN_C_END

#endif

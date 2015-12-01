#ifndef AST_LEXINT_H
#define AST_LEXINT_H

#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct lexint_t
{
  uint64_t low;
  uint64_t high;
} lexint_t;

void lexint_zero(lexint_t* i);

int lexint_cmp(lexint_t* a, lexint_t* b);

int lexint_cmp64(lexint_t* a, uint64_t b);

void lexint_shl(lexint_t* dst, lexint_t* a, uint64_t b);

void lexint_shr(lexint_t* dst, lexint_t* a, uint64_t b);

uint64_t lexint_testbit(lexint_t* a, uint8_t b);

void lexint_setbit(lexint_t* dst, lexint_t* a, uint8_t b);

void lexint_add(lexint_t* dst, lexint_t* a, lexint_t* b);

void lexint_add64(lexint_t* dst, lexint_t* a, uint64_t b);

void lexint_sub(lexint_t* dst, lexint_t* a, lexint_t* b);

void lexint_sub64(lexint_t* dst, lexint_t* a, uint64_t b);

void lexint_mul64(lexint_t* dst, lexint_t* a, uint64_t b);

void lexint_div64(lexint_t* dst, lexint_t* a, uint64_t b);

void lexint_char(lexint_t* i, int c);

bool lexint_accum(lexint_t* i, uint64_t digit, uint64_t base);

double lexint_double(lexint_t* i);

PONY_EXTERN_C_END

#endif

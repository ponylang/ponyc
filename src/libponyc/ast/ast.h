#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include "error.h"
#include "token.h"
#include "symtab.h"
#include "source.h"
#include "stringtab.h"

#include <platform.h>

#if defined(PLATFORM_IS_POSIX_BASED) && defined(__cplusplus)
extern "C" {
#endif

typedef struct ast_t ast_t;

typedef enum
{
  AST_OK,
  AST_IGNORE,
  AST_ERROR,
  AST_FATAL
} ast_result_t;

enum
{
  AST_FLAG_PASS_MASK    = 0x0F,
  AST_FLAG_CAN_ERROR    = 0x20,
  AST_FLAG_CAN_SEND     = 0x40,
  AST_FLAG_MIGHT_SEND   = 0x80,
  AST_FLAG_IN_PARENS    = 0x100,
  AST_FLAG_AMBIGUOUS    = 0x200,
  AST_FLAG_BAD_SEMI     = 0x400,
  AST_FLAG_MISSING_SEMI = 0x800,
  AST_FLAG_PRESERVE     = 0x1000, // Do not process
  AST_FLAG_RECURSE_1    = 0x2000,
  AST_FLAG_DONE_1       = 0x4000,
  AST_FLAG_ERROR_1      = 0x8000,
  AST_FLAG_RECURSE_2    = 0x10000,
  AST_FLAG_DONE_2       = 0x20000,
  AST_FLAG_ERROR_2      = 0x40000,
};


ast_t* ast_new(token_t* t, token_id id);
ast_t* ast_blank(token_id id);
ast_t* ast_token(token_t* t);
ast_t* ast_from(ast_t* ast, token_id id);
ast_t* ast_from_string(ast_t* ast, const char* name);
ast_t* ast_from_int(ast_t* ast, uint64_t value);
ast_t* ast_from_float(ast_t* ast, double value);
ast_t* ast_dup(ast_t* ast);
void ast_scope(ast_t* ast);
bool ast_has_scope(ast_t* ast);
symtab_t* ast_get_symtab(ast_t* ast);
ast_t* ast_setid(ast_t* ast, token_id id);
void ast_setpos(ast_t* ast, source_t* source, size_t line, size_t pos);

token_id ast_id(ast_t* ast);
size_t ast_line(ast_t* ast);
size_t ast_pos(ast_t* ast);
source_t* ast_source(ast_t* ast);

void* ast_data(ast_t* ast);
ast_t* ast_setdata(ast_t* ast, void* data);
bool ast_canerror(ast_t* ast);
void ast_seterror(ast_t* ast);
bool ast_cansend(ast_t* ast);
void ast_setsend(ast_t* ast);
bool ast_mightsend(ast_t* ast);
void ast_setmightsend(ast_t* ast);
void ast_clearmightsend(ast_t* ast);
void ast_inheritflags(ast_t* ast);
int ast_checkflag(ast_t* ast, uint32_t flag);
void ast_setflag(ast_t* ast, uint32_t flag);
void ast_clearflag(ast_t* ast, uint32_t flag);
void ast_resetpass(ast_t* ast);

const char* ast_get_print(ast_t* ast);
const char* ast_name(ast_t* ast);
const char* ast_nice_name(ast_t* ast);
size_t ast_name_len(ast_t* ast);
void ast_set_name(ast_t* ast, const char* name);
double ast_float(ast_t* ast);
lexint_t* ast_int(ast_t* ast);
ast_t* ast_type(ast_t* ast);
void ast_settype(ast_t* ast, ast_t* type);
void ast_erase(ast_t* ast);

ast_t* ast_nearest(ast_t* ast, token_id id);
ast_t* ast_try_clause(ast_t* ast, size_t* clause);

ast_t* ast_parent(ast_t* ast);
ast_t* ast_child(ast_t* ast);
ast_t* ast_childidx(ast_t* ast, size_t idx);
ast_t* ast_childlast(ast_t* ast);
size_t ast_childcount(ast_t* ast);
ast_t* ast_sibling(ast_t* ast);
ast_t* ast_previous(ast_t* ast);
size_t ast_index(ast_t* ast);

ast_t* ast_get(ast_t* ast, const char* name, sym_status_t* status);
ast_t* ast_get_case(ast_t* ast, const char* name, sym_status_t* status);
bool ast_set(ast_t* ast, const char* name, ast_t* value, sym_status_t status);
void ast_setstatus(ast_t* ast, const char* name, sym_status_t status);
void ast_inheritstatus(ast_t* dst, ast_t* src);
void ast_inheritbranch(ast_t* dst, ast_t* src);
void ast_consolidate_branches(ast_t* ast, size_t count);
bool ast_canmerge(ast_t* dst, ast_t* src);
bool ast_merge(ast_t* dst, ast_t* src);
bool ast_within_scope(ast_t* outer, ast_t* inner, const char* name);
bool ast_all_consumes_in_scope(ast_t* outer, ast_t* inner);
void ast_clear(ast_t* ast);
void ast_clear_local(ast_t* ast);

ast_t* ast_add(ast_t* parent, ast_t* child);
ast_t* ast_add_sibling(ast_t* older_sibling, ast_t* new_sibling);
ast_t* ast_pop(ast_t* ast);
ast_t* ast_append(ast_t* parent, ast_t* child);
ast_t* ast_list_append(ast_t* parent, ast_t** last_pointer, ast_t* new_child);
void ast_remove(ast_t* ast);
void ast_swap(ast_t* prev, ast_t* next);
void ast_replace(ast_t** prev, ast_t* next);
void ast_reorder_children(ast_t* ast, const size_t* new_order,
  ast_t** shuffle_space);
void ast_free(ast_t* ast);
void ast_free_unattached(ast_t* ast);

void ast_print(ast_t* ast);
void ast_printverbose(ast_t* ast);
const char* ast_print_type(ast_t* type);
void ast_setwidth(size_t w);

void ast_error(ast_t* ast, const char* fmt, ...)
  __attribute__((format(printf, 2, 3)));
void ast_error_frame(errorframe_t* frame, ast_t* ast, const char* fmt, ...)
  __attribute__((format(printf, 3, 4)));

// Foreach macro, will apply macro M to each of up to 30 other arguments
#define FOREACH(M, ...) \
  EXPAND(FE(__VA_ARGS__, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, \
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, NOP, NOP, NOP, NOP, NOP, NOP, \
    NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, \
    NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP))
#define FE( \
  A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, \
  A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, \
  M0, M1, M2, M3, M4, M5, M6, M7, M8, M9, M10, M11, M12, M13, M14, M15, \
  M16, M17, M18, M19, M20, M21, M22, M23, M24, M25, M26, M27, M28, M29, ...) \
  M0(A0) M1(A1) M2(A2) M3(A3) M4(A4) M5(A5) M6(A6) M7(A7) M8(A8) M9(A9) \
  M10(A10) M11(A11) M12(A12) M13(A13) M14(A14) M15(A15) M16(A16) M17(A17) \
  M18(A18) M19(A19) M20(A20) M21(A21) M22(A22) M23(A23) M24(A24) M25(A25) \
  M26(A26) M27(A27) M28(A28) M29(A29)

#define NOP(x)
// We need this, because MSVC/MSVC++ treats __VA_ARGS__ as single argument if
// passed to another macro
#define EXPAND(x) x

typedef ast_t* ast_ptr_t; // Allows easier decalaration of locals
#define ADDR_AST(x) &x,

void ast_get_children(ast_t* parent, size_t child_count,
  ast_t*** out_children);

#define AST_GET_CHILDREN(parent, ...) \
  ast_ptr_t __VA_ARGS__; \
  AST_GET_CHILDREN_NO_DECL(parent, __VA_ARGS__)

#define AST_GET_CHILDREN_NO_DECL(parent, ...) \
  { \
    ast_t** children[] = { FOREACH(ADDR_AST, __VA_ARGS__) NULL }; \
    ast_get_children(parent, (sizeof(children) / sizeof(ast_t**)) - 1, \
      children); \
  }


void ast_extract_children(ast_t* parent, size_t child_count,
  ast_t*** out_children);

#define AST_EXTRACT_CHILDREN(parent, ...) \
  ast_ptr_t __VA_ARGS__; \
  { \
    ast_t** children[] = { FOREACH(ADDR_AST, __VA_ARGS__) NULL }; \
    ast_extract_children(parent, (sizeof(children)/sizeof(ast_t**)) - 1, \
      children); \
  }


#if defined(PLATFORM_IS_POSIX_BASED) && defined(__cplusplus)
}
#endif

#endif

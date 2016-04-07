#ifndef FRAME_H
#define FRAME_H

#include "ast.h"

PONY_EXTERN_C_BEGIN

typedef struct typecheck_frame_t
{
  ast_t* package;
  ast_t* module;
  ast_t* type;
  ast_t* constraint;
  ast_t* provides;
  ast_t* method;
  ast_t* def_arg;
  ast_t* method_body;
  ast_t* method_params;
  ast_t* method_type;
  ast_t* ffi_type;
  ast_t* local_type;
  ast_t* as_type;
  ast_t* the_case;
  ast_t* pattern;
  ast_t* loop;
  ast_t* loop_cond;
  ast_t* loop_body;
  ast_t* loop_else;
  ast_t* try_expr;
  ast_t* recover;
  ast_t* ifdef_cond;
  ast_t* ifdef_clause;

  struct typecheck_frame_t* prev;
} typecheck_frame_t;

typedef struct typecheck_stats_t
{
  size_t names_count;
  size_t default_caps_count;
  size_t heap_alloc;
  size_t stack_alloc;
} typecheck_stats_t;

typedef struct typecheck_t
{
  typecheck_frame_t* frame;
  typecheck_stats_t stats;
} typecheck_t;

bool frame_push(typecheck_t* t, ast_t* ast);

void frame_pop(typecheck_t* t);

PONY_EXTERN_C_END

#endif

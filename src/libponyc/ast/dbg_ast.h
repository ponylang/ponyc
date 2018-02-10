#ifndef DBG_AST_H
#define DBG_AST_H

#include "../../libponyrt/dbg/dbg.h"
#include "../../libponyrt/dbg/dbg_util.h"
#include "../pass/pass.h"
#include "ast.h"

#include <platform.h>

#if !defined(DBG_AST_ENABLED)
#define DBG_AST_ENABLED false
#endif

#if !defined(DBG_AST_PRINT_WIDTH)
#define DBG_AST_PRINT_WIDTH 200
#endif

#define DBG_AST(ctx, bit_idx, ast) \
  _DBG_DO( \
    if(DBG_AST_ENABLED) \
      if(dbg_gb(ctx, bit_idx)) \
      { \
        fprintf(ctx->dst_file, "%s:  %s ", __FUNCTION__, dbg_str(ast)); \
        ast_fprint(ctx->dst_file, ast, DBG_AST_PRINT_WIDTH); \
      })

#define DBG_ASTF(ctx, bit_idx, ast, format, ...) \
  _DBG_DO( \
    if(DBG_AST_ENABLED) \
      if(dbg_gb(ctx, bit_idx)) \
      { \
        fprintf(ctx->dst_file, "%s:  " format, __FUNCTION__, ## __VA_ARGS__); \
        ast_fprint(ctx->dst_file, ast, DBG_AST_PRINT_WIDTH); \
      })

// Debug ast_print_and_parents
#define DBG_ASTP(ctx, bit_idx, ast, number) \
  _DBG_DO( \
    if(DBG_AST_ENABLED) \
      if(dbg_gb(ctx, bit_idx)) \
        for(int i=0; (ast != NULL) && (i < number); i++) \
        { \
          fprintf(ctx->dst_file, "%s: %s[%d]: ", __FUNCTION__, dbg_str(ast), -i); \
          ast_fprint(ctx->dst_file, ast, DBG_AST_PRINT_WIDTH); \
          ast = ast_parent(ast); \
        })

// Debug ast_siblings
#define DBG_ASTS(ctx, bit_idx, ast) \
  _DBG_DO( \
    if(DBG_AST_ENABLED) \
      if(dbg_gb(ctx, bit_idx)) \
      { \
        ast_t* parent = ast_parent(ast); \
        ast_t* child = ast_child(parent); \
        for(int i=0; (child != NULL); i++) \
        { \
          fprintf(ctx->dst_file, "%s %s[%d]: ", __FUNCTION__, dbg_str(ast), i); \
          ast_fprint(ctx->dst_file, child, DBG_AST_PRINT_WIDTH); \
          child = ast_sibling(child); \
        } \
      })

#endif

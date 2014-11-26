#include "pass.h"
#include "../codegen/codegen.h"
#include "parsefix.h"
#include "sugar.h"
#include "scope.h"
#include "names.h"
#include "traits.h"
#include "expr.h"
#include "../../libponyrt/mem/pool.h"

#include <string.h>
#include <stdbool.h>


static pass_id pass_limit = PASS_ALL;

bool limit_passes(const char* pass)
{
  for(pass_id i = PASS_PARSE; i <= PASS_ALL; i++)
  {
    if(strcmp(pass, pass_name(i)) == 0)
    {
      pass_limit = i;
      return true;
    }
  }

  return false;
}


const char* pass_name(pass_id pass)
{
  switch(pass)
  {
    case PASS_PARSE:    return "parse";
    case PASS_PARSE_FIX:return "parsefix";
    case PASS_SUGAR:    return "sugar";
    case PASS_SCOPE1:   return "scope1";
    case PASS_NAME_RESOLUTION: return "name";
    case PASS_FLATTEN:  return "flatten";
    case PASS_TRAITS:   return "traits";
    case PASS_SCOPE2:   return "scope2";
    case PASS_EXPR:     return "expr";
    case PASS_AST:      return "ast";
    case PASS_LLVM_IR:  return "ir";
    case PASS_BITCODE:  return "bitcode";
    case PASS_ASM:      return "asm";
    case PASS_OBJ:      return "obj";
    case PASS_ALL:      return "all";
    default:            return "error";
  }
}


void pass_opt_init(pass_opt_t* options)
{
  memset(options, 0, sizeof(pass_opt_t));

  // Start with an empty typechecker frame.
  options->check.frame = POOL_ALLOC(typecheck_frame_t);
  memset(options->check.frame, 0, sizeof(typecheck_frame_t));
}


void pass_opt_done(pass_opt_t* options)
{
  // Pop all the typechecker frames.
  while(options->check.frame != NULL)
  {
    typecheck_frame_t* f = options->check.frame;
    options->check.frame = f->prev;
    POOL_FREE(typecheck_frame_t, f);
  }
}



// Do a single pass, if the limit allows
static bool do_pass(ast_t** astp, bool* out_result, pass_opt_t* options,
  pass_id pass, ast_visit_t pre_fn, ast_visit_t post_fn)
{
  if(pass_limit < pass)
  {
    *out_result = true;
    return true;
  }

  if(ast_visit(astp, pre_fn, post_fn, options) != AST_OK)
  {
    *out_result = false;
    return true;
  }

  return false;
}


bool package_passes(ast_t* package, pass_opt_t* options)
{
  if(pass_limit == PASS_PARSE)
    return true;

  bool r;

  if(do_pass(&package, &r, options, PASS_PARSE_FIX, pass_parse_fix, NULL))
    return r;

  if(do_pass(&package, &r, options, PASS_SUGAR, pass_sugar, NULL))
    return r;

  if(do_pass(&package, &r, options, PASS_SCOPE1, pass_scope, NULL))
    return r;

  if(do_pass(&package, &r, options, PASS_NAME_RESOLUTION, NULL, pass_names))
    return r;

  if(do_pass(&package, &r, options, PASS_FLATTEN, NULL, pass_flatten))
    return r;

  if(do_pass(&package, &r, options, PASS_TRAITS, pass_traits, NULL))
    return r;

  if(pass_limit != PASS_TRAITS)
    ast_clear(package);

  if(do_pass(&package, &r, options, PASS_SCOPE2, pass_scope2, NULL))
    return r;

  if(do_pass(&package, &r, options, PASS_EXPR, NULL, pass_expr))
    return r;

  return true;
}


bool program_passes(ast_t* program, pass_opt_t* opt)
{
  if(pass_limit < PASS_LLVM_IR)
    return true;

  return codegen(program, opt, pass_limit);
}

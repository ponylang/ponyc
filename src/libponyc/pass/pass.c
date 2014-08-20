#include "pass.h"
#include "../codegen/codegen.h"
#include "sugar.h"
#include "scope.h"
#include "names.h"
#include "traits.h"
#include "expr.h"

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
    case PASS_SUGAR:    return "sugar";
    case PASS_SCOPE1:   return "scope1";
    case PASS_NAME_RESOLUTION: return "name";
    case PASS_FLATTEN:  return "flatten";
    case PASS_TRAITS:   return "traits";
    case PASS_SCOPE2:   return "scope2";
    case PASS_EXPR:     return "expr";
    case PASS_ALL:      return "all";
    default:            return "error";
  }
}


// Do a single pass, if the limit allows
static bool do_pass(ast_t** astp, bool* out_result, pass_id pass,
  ast_visit_t pre_fn, ast_visit_t post_fn)
{
  if(pass_limit < pass)
  {
    *out_result = true;
    return true;
  }

  if(ast_visit(astp, pre_fn, post_fn) != AST_OK)
  {
    *out_result = false;
    return true;
  }

  return false;
}


bool package_passes(ast_t* package)
{
  if(pass_limit == PASS_PARSE)
    return true;

  bool r;

  if(do_pass(&package, &r, PASS_SUGAR, pass_sugar, NULL))
    return r;

  if(do_pass(&package, &r, PASS_SCOPE1, pass_scope, NULL))
    return r;

  if(do_pass(&package, &r, PASS_NAME_RESOLUTION, NULL, pass_names))
    return r;

  if(do_pass(&package, &r, PASS_FLATTEN, NULL, pass_flatten))
    return r;

  if(do_pass(&package, &r, PASS_TRAITS, pass_traits, NULL))
    return r;

  if(pass_limit != PASS_TRAITS)
    ast_clear(package);

  if(do_pass(&package, &r, PASS_SCOPE2, pass_scope, NULL))
    return r;

  if(do_pass(&package, &r, PASS_EXPR, NULL, pass_expr))
    return r;

  return true;
}


bool program_passes(ast_t* program, int opt, bool print_llvm)
{
  if(pass_limit < PASS_ALL)
    return true;
  
  return codegen(program, opt, print_llvm);
}

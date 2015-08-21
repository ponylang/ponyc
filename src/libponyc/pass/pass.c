#include "pass.h"
#include "../ast/parser.h"
#include "syntax.h"
#include "sugar.h"
#include "scope.h"
#include "import.h"
#include "names.h"
#include "flatten.h"
#include "traits.h"
#include "expr.h"
#include "finalisers.h"
#include "docgen.h"
#include "../codegen/codegen.h"
#include "../../libponyrt/mem/pool.h"

#include <string.h>
#include <stdbool.h>
#include <assert.h>


bool limit_passes(pass_opt_t* opt, const char* pass)
{
  pass_id i = PASS_PARSE;
  
  while(true)
  {
    if(strcmp(pass, pass_name(i)) == 0)
    {
      opt->limit = i;
      return true;
    }

    if(i == PASS_ALL)
      return false;

    i = pass_next(i);
  }
}


const char* pass_name(pass_id pass)
{
  switch(pass)
  {
    case PASS_PARSE: return "parse";
    case PASS_SYNTAX: return "syntax";
    case PASS_SUGAR: return "sugar";
    case PASS_SCOPE: return "scope";
    case PASS_IMPORT: return "import";
    case PASS_NAME_RESOLUTION: return "name";
    case PASS_FLATTEN: return "flatten";
    case PASS_TRAITS: return "traits";
    case PASS_DOCS: return "docs";
    case PASS_EXPR: return "expr";
    case PASS_FINALISER: return "final";
    case PASS_LLVM_IR: return "ir";
    case PASS_BITCODE: return "bitcode";
    case PASS_ASM: return "asm";
    case PASS_OBJ: return "obj";
    case PASS_ALL: return "all";
    default: return "error";
  }
}


pass_id pass_next(pass_id pass)
{
  if(pass == PASS_ALL)  // Limit end of list
    return PASS_ALL;

  return (pass_id)(pass + 1);
}


pass_id pass_prev(pass_id pass)
{
  if(pass == PASS_PARSE)  // Limit start of list
    return PASS_PARSE;

  return (pass_id)(pass - 1);
}


void pass_opt_init(pass_opt_t* options)
{
  // Start with an empty typechecker frame.
  memset(options, 0, sizeof(pass_opt_t));
  options->limit = PASS_ALL;
  frame_push(&options->check, NULL);
}


void pass_opt_done(pass_opt_t* options)
{
  // Pop all the typechecker frames.
  while(options->check.frame != NULL)
    frame_pop(&options->check);

  if(options->print_stats)
  {
    printf(
      "\nStats:"
      "\n  Names: " __zu
      "\n  Default caps: " __zu
      "\n  Heap alloc: " __zu
      "\n  Stack alloc: " __zu
      "\n",
      options->check.stats.names_count,
      options->check.stats.default_caps_count,
      options->check.stats.heap_alloc,
      options->check.stats.stack_alloc
      );
  }
}


// Check whether we have reached the maximum pass we should currently perform.
// We check against both the specified last pass and the limit set in the
// options, if any.
// Returns true if we should perform the specified pass, false if we shouldn't.
static bool check_limit(ast_t** astp, pass_opt_t* options, pass_id pass,
  pass_id last_pass)
{
  assert(astp != NULL);
  assert(*astp != NULL);
  assert(options != NULL);

  if(last_pass < pass || options->limit < pass)
    return false;

  if(ast_id(*astp) == TK_PROGRAM) // Record pass program AST has reached
    options->program_pass = pass;

  return true;
}


// Perform an ast_visit pass, after checking the pass limits
static bool visit_pass(ast_t** astp, pass_opt_t* options, pass_id last_pass,
  pass_id pass, ast_visit_t pre_fn, ast_visit_t post_fn)
{
  if(!check_limit(astp, options, pass, last_pass))
    return true;

  return ast_visit(astp, pre_fn, post_fn, options) == AST_OK;
}


bool module_passes(ast_t* package, pass_opt_t* options, source_t* source)
{
  if(!pass_parse(package, source))
    return false;

  if(options->limit < PASS_SYNTAX)
    return true;

  ast_t* module = ast_child(package);
  return ast_visit(&module, pass_syntax, NULL, options) == AST_OK;
}


// Peform the AST passes on the given AST up to the speficied last pass
static bool ast_passes(ast_t** astp, pass_opt_t* options, pass_id last)
{
  assert(astp != NULL);

  if(!visit_pass(astp, options, last, PASS_SUGAR, pass_sugar, NULL))
    return false;

  if(!visit_pass(astp, options, last, PASS_SCOPE, pass_scope, NULL))
    return false;

  if(!visit_pass(astp, options, last, PASS_IMPORT, pass_import, NULL))
    return false;

  if(!visit_pass(astp, options, last, PASS_NAME_RESOLUTION, NULL, pass_names))
    return false;

  if(!visit_pass(astp, options, last, PASS_FLATTEN, NULL, pass_flatten))
    return false;

  if(!visit_pass(astp, options, last, PASS_TRAITS, pass_traits, NULL))
    return false;

  if(check_limit(astp, options, PASS_DOCS, last))
  {
    if(options->docs && ast_id(*astp) == TK_PROGRAM)
      generate_docs(*astp, options);
  }

  if(!visit_pass(astp, options, last, PASS_EXPR, pass_pre_expr, pass_expr))
    return false;

  if(check_limit(astp, options, PASS_FINALISER, last))
  {
    if(!pass_finalisers(*astp))
      return false;
  }

  return true;
}


bool ast_passes_program(ast_t* ast, pass_opt_t* options)
{
  return ast_passes(&ast, options, PASS_ALL);
}


bool ast_passes_type(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  
  assert(ast_id(ast) == TK_ACTOR || ast_id(ast) == TK_CLASS ||
    ast_id(ast) == TK_PRIMITIVE || ast_id(ast) == TK_TRAIT ||
    ast_id(ast) == TK_INTERFACE);

  // We don't have the right frame stack for an entity, set up appropriate
  // frames
  ast_t* module = ast_parent(ast);
  ast_t* package = ast_parent(module);

  frame_push(&options->check, NULL);
  frame_push(&options->check, package);
  frame_push(&options->check, module);

  bool ok = ast_passes(astp, options, options->program_pass);

  frame_pop(&options->check);
  frame_pop(&options->check);
  frame_pop(&options->check);

  return ok;
}


bool ast_passes_subtree(ast_t** astp, pass_opt_t* options, pass_id last_pass)
{
  return ast_passes(astp, options, last_pass);
}


bool generate_passes(ast_t* program, pass_opt_t* options)
{
  if(options->limit < PASS_LLVM_IR)
    return true;

  return codegen(program, options);
}

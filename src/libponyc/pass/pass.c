#include "pass.h"
#include "../ast/parser.h"
#include "../ast/treecheck.h"
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

  if(ast_id(*astp) == TK_PROGRAM) // Update pass for catching up to
    options->program_pass = pass;

  return true;
}


// Perform an ast_visit pass, after checking the pass limits.
// Returns true to continue, false to stop processing and return the value in
// out_r.
static bool visit_pass(ast_t** astp, pass_opt_t* options, pass_id last_pass,
  bool* out_r, pass_id pass, ast_visit_t pre_fn, ast_visit_t post_fn)
{
  assert(out_r != NULL);

  if(!check_limit(astp, options, pass, last_pass))
  {
    *out_r = true;
    return false;
  }

  //printf("Pass %s (last %s) on %s\n", pass_name(pass), pass_name(last_pass),
  //  ast_get_print(*astp));

  if(ast_visit(astp, pre_fn, post_fn, options, pass) != AST_OK)
  {
    *out_r = false;
    return false;
  }

  return true;
}


bool module_passes(ast_t* package, pass_opt_t* options, source_t* source)
{
  if(!pass_parse(package, source))
    return false;

  if(options->limit < PASS_SYNTAX)
    return true;

  ast_t* module = ast_child(package);

  if(ast_visit(&module, pass_syntax, NULL, options, PASS_SYNTAX) != AST_OK)
    return false;

  check_tree(module);
  return true;
}


// Peform the AST passes on the given AST up to the speficied last pass
static bool ast_passes(ast_t** astp, pass_opt_t* options, pass_id last)
{
  assert(astp != NULL);
  bool r;

  if(!visit_pass(astp, options, last, &r, PASS_SUGAR, pass_sugar, NULL))
    return r;

  check_tree(*astp);

  if(!visit_pass(astp, options, last, &r, PASS_SCOPE, pass_scope, NULL))
    return r;

  if(!visit_pass(astp, options, last, &r, PASS_IMPORT, pass_import, NULL))
    return r;

  if(!visit_pass(astp, options, last, &r, PASS_NAME_RESOLUTION, NULL,
    pass_names))
    return r;

  if(!visit_pass(astp, options, last, &r, PASS_FLATTEN, NULL, pass_flatten))
    return r;

  if(!visit_pass(astp, options, last, &r, PASS_TRAITS, pass_traits, NULL))
    return r;

  if(!check_limit(astp, options, PASS_DOCS, last))
    return true;

  if(options->docs && ast_id(*astp) == TK_PROGRAM)
    generate_docs(*astp, options);

  if(!visit_pass(astp, options, last, &r, PASS_EXPR, pass_pre_expr, pass_expr))
    return r;

  if(!check_limit(astp, options, PASS_FINALISER, last))
    return true;

  if(!pass_finalisers(*astp))
    return false;

  check_tree(*astp);
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
    ast_id(ast) == TK_STRUCT || ast_id(ast) == TK_PRIMITIVE ||
    ast_id(ast) == TK_TRAIT || ast_id(ast) == TK_INTERFACE);

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


static void record_ast_pass(ast_t* ast, pass_id pass)
{
  assert(ast != NULL);

  if(pass == PASS_ALL)
    return;

  ast_clearflag(ast, AST_FLAG_PASS_MASK);
  ast_setflag(ast, (int)pass);
}


ast_result_t ast_visit(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options, pass_id pass)
{
  assert(ast != NULL);
  assert(*ast != NULL);

  pass_id ast_pass = (pass_id)ast_checkflag(*ast, AST_FLAG_PASS_MASK);

  if(ast_pass >= pass)  // This pass already done for this AST node
    return AST_OK;

  if(ast_checkflag(*ast, AST_FLAG_PRESERVE))  // Do not process this subtree
    return AST_OK;

  typecheck_t* t = &options->check;
  bool pop = frame_push(t, *ast);

  ast_result_t ret = AST_OK;
  bool ignore = false;

  if(pre != NULL)
  {
    switch(pre(ast, options))
    {
      case AST_OK:
        break;

      case AST_IGNORE:
        ignore = true;
        break;

      case AST_ERROR:
        ret = AST_ERROR;
        break;

      case AST_FATAL:
        record_ast_pass(*ast, pass);
        return AST_FATAL;
    }
  }

  if(!ignore && ((pre != NULL) || (post != NULL)))
  {
    ast_t* child = ast_child(*ast);

    while(child != NULL)
    {
      switch(ast_visit(&child, pre, post, options, pass))
      {
        case AST_OK:
          break;

        case AST_IGNORE:
          // Can never happen
          assert(0);
          break;

        case AST_ERROR:
          ret = AST_ERROR;
          break;

        case AST_FATAL:
          record_ast_pass(*ast, pass);
          return AST_FATAL;
      }

      child = ast_sibling(child);
    }
  }

  if(!ignore && post != NULL)
  {
    switch(post(ast, options))
    {
      case AST_OK:
      case AST_IGNORE:
        break;

      case AST_ERROR:
        ret = AST_ERROR;
        break;

      case AST_FATAL:
        record_ast_pass(*ast, pass);
        return AST_FATAL;
    }
  }

  if(pop)
    frame_pop(t);

  record_ast_pass(*ast, pass);
  return ret;
}


ast_result_t ast_visit_scope(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options, pass_id pass)
{
  typecheck_t* t = &options->check;
  bool pop = frame_push(t, NULL);

  ast_result_t ret = ast_visit(ast, pre, post, options, pass);

  if(pop)
    frame_pop(t);

  return ret;
}

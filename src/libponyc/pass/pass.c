#include "pass.h"
#include "syntax.h"
#include "sugar.h"
#include "scope.h"
#include "import.h"
#include "names.h"
#include "flatten.h"
#include "traits.h"
#include "refer.h"
#include "expr.h"
#include "completeness.h"
#include "verify.h"
#include "finalisers.h"
#include "serialisers.h"
#include "dartsrc.h"
#include "docgen.h"
#include "../ast/ast.h"
#include "../ast/parser.h"
#include "../ast/treecheck.h"
#include "../codegen/codegen.h"
#include "../pkg/program.h"
#include "../plugin/plugin.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


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
    case PASS_REFER: return "refer";
    case PASS_EXPR: return "expr";
    case PASS_COMPLETENESS: return "completeness";
    case PASS_VERIFY: return "verify";
    case PASS_FINALISER: return "final";
    case PASS_SERIALISER: return "serialise";
    case PASS_DARTSRC: return "dartsrc";
    case PASS_REACH: return "reach";
    case PASS_PAINT: return "paint";
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
  options->verbosity = VERBOSITY_INFO;
  options->check.errors = errors_alloc();
  options->ast_print_width = 80;
  frame_push(&options->check, NULL);
}


void pass_opt_done(pass_opt_t* options)
{
  plugin_unload(options);

  // Free the error collection.
  errors_free(options->check.errors);
  options->check.errors = NULL;

  // Pop the initial typechecker frame.
  frame_pop(&options->check);
  pony_assert(options->check.frame == NULL);

  if(options->print_stats)
  {
    fprintf(stderr,
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
  pony_assert(astp != NULL);
  pony_assert(*astp != NULL);
  pony_assert(options != NULL);

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
  pony_assert(out_r != NULL);

  if(!check_limit(astp, options, pass, last_pass))
  {
    *out_r = true;
    return false;
  }

  //fprintf(stderr, "Pass %s (last %s) on %s\n", pass_name(pass),
  //  pass_name(last_pass), ast_get_print(*astp));

  if(ast_visit(astp, pre_fn, post_fn, options, pass) != AST_OK)
  {
    *out_r = false;
    return false;
  }

  return true;
}


bool module_passes(ast_t* package, pass_opt_t* options, source_t* source)
{
  if(!pass_parse(package, source, options->check.errors,
    options->allow_test_symbols, options->parse_trace))
    return false;

  if(options->limit < PASS_SYNTAX)
    return true;

  ast_t* module = ast_child(package);

  if(ast_visit(&module, pass_syntax, NULL, options, PASS_SYNTAX) != AST_OK)
    return false;

  if(options->check_tree)
    check_tree(module, options);
  return true;
}


// Peform the AST passes on the given AST up to the specified last pass
static bool ast_passes(ast_t** astp, pass_opt_t* options, pass_id last)
{
  pony_assert(astp != NULL);
  bool r;
  bool is_program = ast_id(*astp) == TK_PROGRAM;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_SYNTAX);

  if(!visit_pass(astp, options, last, &r, PASS_SUGAR, pass_sugar, NULL))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_SUGAR);

  if(options->check_tree)
    check_tree(*astp, options);

  if(!visit_pass(astp, options, last, &r, PASS_SCOPE, pass_scope, NULL))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_SCOPE);

  if(!visit_pass(astp, options, last, &r, PASS_IMPORT, pass_import, NULL))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_IMPORT);

  if(!visit_pass(astp, options, last, &r, PASS_NAME_RESOLUTION, NULL,
    pass_names))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_NAME_RESOLUTION);

  if(!visit_pass(astp, options, last, &r, PASS_FLATTEN, NULL, pass_flatten))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_FLATTEN);

  if(!visit_pass(astp, options, last, &r, PASS_TRAITS, pass_traits, NULL))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_TRAITS);

  if(!check_limit(astp, options, PASS_DOCS, last))
    return true;

  if(is_program && options->docs)
    generate_docs(*astp, options);

  if(!visit_pass(astp, options, last, &r, PASS_REFER, pass_pre_refer,
    pass_refer))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_REFER);

  if(!visit_pass(astp, options, last, &r, PASS_EXPR, pass_pre_expr, pass_expr))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_EXPR);

  if(!visit_pass(astp, options, last, &r, PASS_COMPLETENESS, NULL,
    pass_completeness))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_COMPLETENESS);

  if(!visit_pass(astp, options, last, &r, PASS_VERIFY, NULL, pass_verify))
    return r;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_VERIFY);

  if(!check_limit(astp, options, PASS_FINALISER, last))
    return true;

  if(!pass_finalisers(*astp, options))
    return false;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_FINALISER);

  if(!check_limit(astp, options, PASS_SERIALISER, last))
    return true;

  if(!pass_serialisers(*astp, options))
    return false;

  if(is_program)
    plugin_visit_ast(*astp, options, PASS_SERIALISER);
  
  if(options->check_tree)
    check_tree(*astp, options);

  ast_freeze(*astp);

  if(is_program)
  {
    program_signature(*astp);

    if(options->verbosity >= VERBOSITY_TOOL_INFO)
      program_dump(*astp);
  }

  return true;
}


bool ast_passes_program(ast_t* ast, pass_opt_t* options)
{
  return ast_passes(&ast, options, PASS_ALL);
}


bool ast_passes_type(ast_t** astp, pass_opt_t* options, pass_id last_pass)
{
  ast_t* ast = *astp;

  pony_assert(ast_id(ast) == TK_ACTOR || ast_id(ast) == TK_CLASS ||
    ast_id(ast) == TK_STRUCT || ast_id(ast) == TK_PRIMITIVE ||
    ast_id(ast) == TK_TRAIT || ast_id(ast) == TK_INTERFACE);

  // We don't have the right frame stack for an entity, set up appropriate
  // frames
  ast_t* module = ast_parent(ast);
  ast_t* package = ast_parent(module);

  frame_push(&options->check, NULL);
  frame_push(&options->check, package);
  frame_push(&options->check, module);

  bool ok = ast_passes(astp, options, last_pass);

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
  // called by ponyc/main.c:67 compile_package().
  printf("top of pass.c generate_passes(); options->limit = '%d' vs PASS_REACH = '%d'\n", options->limit, PASS_REACH);
    
  if(options->limit < PASS_REACH)
    return true;

  return codegen(program, options);
}


void ast_pass_record(ast_t* ast, pass_id pass)
{
  pony_assert(ast != NULL);

  if(pass == PASS_ALL)
    return;

  ast_clearflag(ast, AST_FLAG_PASS_MASK);
  ast_setflag(ast, (int)pass);
}


ast_result_t ast_visit(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options, pass_id pass)
{
  pony_assert(ast != NULL);
  pony_assert(*ast != NULL);

  pass_id ast_pass = (pass_id)ast_checkflag(*ast, AST_FLAG_PASS_MASK);

  if(ast_pass >= pass)  // This pass already done for this AST node
    return AST_OK;

  // Do not process this subtree
  if((pass > PASS_SYNTAX) && ast_checkflag(*ast, AST_FLAG_PRESERVE))
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
        ast_pass_record(*ast, pass);

        if(pop)
          frame_pop(t);

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
          pony_assert(0);
          break;

        case AST_ERROR:
          ret = AST_ERROR;
          break;

        case AST_FATAL:
          ast_pass_record(*ast, pass);

          if(pop)
            frame_pop(t);

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
        ast_pass_record(*ast, pass);

        if(pop)
          frame_pop(t);

        return AST_FATAL;
    }
  }

  if(pop)
    frame_pop(t);

  ast_pass_record(*ast, pass);
  return ret;
}


ast_result_t ast_visit_scope(ast_t** ast, ast_visit_t pre, ast_visit_t post,
  pass_opt_t* options, pass_id pass)
{
  typecheck_t* t = &options->check;
  ast_t* module = ast_nearest(*ast, TK_MODULE);
  ast_t* package = ast_parent(module);
  pony_assert(module != NULL);
  pony_assert(package != NULL);

  frame_push(t, NULL);
  frame_push(t, package);
  frame_push(t, module);

  ast_result_t ret = ast_visit(ast, pre, post, options, pass);

  frame_pop(t);
  frame_pop(t);
  frame_pop(t);

  return ret;
}

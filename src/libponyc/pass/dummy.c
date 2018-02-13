#include "dummy.h"
#include "../ast/id.h"
#include "../pass/expr.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../libponyrt/dbg/dbg_util.h"

// Uncomment to enable
#define DBG_ENABLED true
#include "../../libponyrt/dbg/dbg.h"

// Uncomment to enable AST debug
#define DBG_AST_ENABLED true
#include "../ast/dbg_ast.h"

// Initialize debug context
static dbg_ctx_t* dc = NULL;
INITIALIZER(initialize)
{
  if(dc == NULL)
    dc = dbg_ctx_create_with_dst_file(32, stderr);
}

// Finalize debug context
FINALIZER(finalize)
{
  if(dc != NULL)
    dbg_ctx_destroy(dc);
  dc = NULL;
}

static inline void maybe_enable_debug(ast_t* ast, pass_opt_t* opt)
{
  // If initialize didn't run, for instance in Windows, do it now.
  if(dc == NULL)
    initialize();

  UNUSED(opt);
  switch(ast_id(ast))
  {
    case TK_CLASS:
    {
      const char* name = ast_name(ast_child(ast));
      if(strcmp(name, "C") == 0)
      {
        // Uncomment if enter/exit debug desired
        //dbg_sb(dc, 0, 1);
        //dbg_sb(dc, 1, 1);
        dbg_sb(dc, 2, 1);
        DBG_PFNU(dc, "TK_CLASS name=\"%s\"\n", name);
      }
      break;
    }
    default: {}
  }
}

static inline void maybe_disable_debug(ast_t* ast, pass_opt_t* opt)
{
  UNUSED(opt);
  switch(ast_id(ast))
  {
    case TK_CLASS:
    {
      const char* name = ast_name(ast_child(ast));

      // The target string could be comming from the command line?
      const char* target = "C";
      if(strcmp(name, target) == 0)
      {
        dbg_sb(dc, 0, 0);
        dbg_sb(dc, 1, 0);
        dbg_sb(dc, 2, 0);
        DBG_PFNU(dc, "TK_CLASS name=\"%s\"\n", name);
        finalize();
      }
      break;
    }
    default: {}
  }
}

ast_result_t pass_pre_dummy(ast_t** astp, pass_opt_t* options)
{
  maybe_enable_debug(*astp, options);

  DBG_E(dc, 0);

  ast_t* ast = *astp;
  switch(ast_id(ast))
  {
    case TK_CLASS: // Windows insists you should have cases
    default: { DBG_ASTF(dc, 2, ast, "id=%d default ", ast_id(ast)); }
  }

  DBG_PFX(dc, 1, "r=%d\n", AST_OK);
  return AST_OK;
}

ast_result_t pass_dummy(ast_t** astp, pass_opt_t* options)
{
  DBG_E(dc, 0);

  ast_t* ast = *astp;
  bool r = true;
  switch(ast_id(ast))
  {
    // Add TK_xx cases here such as
    case TK_REFERENCE: r = true; break;

    default: { DBG_ASTF(dc, 2, ast, "id=%d default ", ast_id(ast)); }
  }

  ast_result_t result;
  if(r)
  {
    result = AST_OK;
  } else {
    pony_assert(errors_get_count(options->check.errors) > 0);
    result = AST_ERROR;
  }
  DBG_PFX(dc, 1, "r=%d\n", result);

  maybe_disable_debug(*astp, options);
  return result;
}

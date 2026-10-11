#include "import.h"
#include "../ast/ast.h"
#include "../ast/symtab.h"
#include "../ast/id.h"
#include "ponyassert.h"
#include <string.h>


static bool import_use(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast != NULL);

  if(!ast_checkflag(ast, AST_FLAG_IMPORT))
    return true;

  ast_t* import = (ast_t*)ast_data(ast);
  pony_assert(import != NULL);

  ast_t* file = ast_parent(ast);
  pony_assert(ast_id(file) == TK_FILE);
  symtab_t* file_symtab = ast_get_symtab(file);

  ast_t* module = ast_parent(file);
  pony_assert(ast_id(module) == TK_MODULE);
  symtab_t* mod_symtab = ast_get_symtab(module);

  ast_t* uri_child = ast_childidx(ast, 1);
  pony_assert(uri_child != NULL);
  const char* uri = ast_name(uri_child);
  pony_assert(uri != NULL);

  bool is_builtin = (strcmp(uri, "builtin") == 0);
  bool ok = true;

  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;
  symtab_t* src_symtab = ast_get_symtab(import);
  pony_assert(src_symtab != NULL);

  while((sym = symtab_next(src_symtab, &i)) != NULL)
  {
    if(is_name_private(sym->name) || // Public symbols only.
      is_name_ffi(sym->name) ||  // Don't merge FFI declarations.
      (sym->status == SYM_NOCASE) ||
      (strcmp(sym->name, "Main") == 0))
      continue;

    ast_t* existing = symtab_find_case(mod_symtab, sym->name, NULL,
      opt->strtab);

    if(existing == NULL)
      existing = symtab_find_case(file_symtab, sym->name, NULL, opt->strtab);

    if(existing != NULL)
    {
      // Symbol clash.
      if(is_builtin)
      {
        ast_error(opt->check.errors, existing,
          "type name clashes with builtin type");
        ast_error_continue(opt->check.errors, sym->def, "builtin type here");
      }
      else
      {
        if(ok)
        {
          // Only print the "use" as an error once.
          ast_error(opt->check.errors, ast,
            "can't use '%s' without alias, clashing symbols", uri);
        }

        ast_error(opt->check.errors, existing,
          "existing type name clashes with type from '%s'", uri);
        ast_error_continue(opt->check.errors, sym->def,
          "clash trying to use this type");
      }

      ok = false;
    }
    else
    {
      // OK to add symbol.
      symtab_add(file_symtab, sym->name, sym->def, sym->status, opt->strtab);
    }

  }

  return ok;
}


ast_result_t pass_import(ast_t** astp, pass_opt_t* options)
{
  (void)options;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_PROGRAM:
    case TK_MODULE:
    case TK_FILE:
      return AST_OK;

    case TK_USE:
      if(!import_use(options, ast))
        return AST_FATAL;
      break;

    default:
      return AST_IGNORE;
  }

  return AST_OK;
}

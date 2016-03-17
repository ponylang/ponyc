#include "import.h"
#include "../ast/ast.h"
#include "../ast/symtab.h"
#include "../pass/names.h"
#include <assert.h>
#include <string.h>


static bool import_use(ast_t* ast)
{
  assert(ast != NULL);

  ast_t* import = (ast_t*)ast_data(ast);

  if(import == NULL) // Nothing to import.
    return true;

  ast_t* module = ast_parent(ast);
  assert(ast_id(module) == TK_MODULE);
  symtab_t* mod_symtab = ast_get_symtab(module);

  ast_t* package = ast_parent(module);
  assert(ast_id(package) == TK_PACKAGE);
  symtab_t* pkg_symtab = ast_get_symtab(package);

  ast_t* uri_child = ast_childidx(ast, 1);
  assert(uri_child != NULL);
  const char* uri = ast_name(uri_child);
  assert(uri != NULL);

  bool is_builtin = (strcmp(uri, "builtin") == 0);
  bool ok = true;

  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;
  symtab_t* src_symtab = ast_get_symtab(import);
  assert(src_symtab != NULL);

  while((sym = symtab_next(src_symtab, &i)) != NULL)
  {
    if(is_name_private(sym->name) || // Public symbols only.
      is_name_ffi(sym->name) ||  // Don't merge FFI declarations.
      (sym->status == SYM_NOCASE) ||
      (strcmp(sym->name, "Main") == 0))
      continue;

    ast_t* existing = symtab_find_case(pkg_symtab, sym->name, NULL);

    if(existing == NULL)
      existing = symtab_find_case(mod_symtab, sym->name, NULL);

    if(existing != NULL)
    {
      // Symbol clash.
      if(is_builtin)
      {
        ast_error(existing, "type name clashes with builtin type");
        ast_error(sym->def, "builtin type here");
      }
      else
      {
        if(ok)
        {
          // Only print the "use" as an error once.
          ast_error(ast, "can't use '%s' without alias, clashing symbols",
            uri);
        }

        ast_error(existing, "existing type name clashes with type from '%s'",
          uri);
        ast_error(sym->def, "clash trying to use this type");
      }

      ok = false;
    }
    else
    {
      // OK to add symbol.
      symtab_add(mod_symtab, sym->name, sym->def, sym->status);
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
    case TK_PACKAGE:
    case TK_MODULE:
      return AST_OK;

    case TK_USE:
      if(!import_use(ast))
        return AST_FATAL;
      break;

    default:
      return AST_IGNORE;
  }

  return AST_OK;
}

#include "typechecker.h"
#include "type_scope.h"
#include "type_valid.h"
#include "type_expr.h"
#include <assert.h>

bool typecheck(ast_t* ast, int verbose)
{
  if(!ast_visit(ast, type_scope, NULL, verbose))
    return false;

  if(!ast_visit(ast, NULL, type_valid, verbose))
    return false;

  if(!ast_visit(ast, type_traits, NULL, verbose))
    return false;

  if(!ast_visit(ast, NULL, type_expr, verbose))
    return false;

  return true;
}

bool is_type_id(const char* s)
{
  int i = 0;

  if(s[i] == '_')
    i++;

  return (s[i] >= 'A') && (s[i] <= 'Z');
}

ast_t* nominal_def(ast_t* nominal)
{
  assert(ast_id(nominal) == TK_NOMINAL);
  ast_t* def = ast_data(nominal);

  if(def != NULL)
    return def;

  ast_t* type_name = ast_child(nominal);
  ast_t* scope = type_name;
  ast_t* id = ast_child(type_name);

  while(id != NULL)
  {
    switch(ast_id(id))
    {
      case TK_THIS:
      {
        // TODO: cap
        if(scope != type_name)
        {
          ast_error(id,
            "viewpoint adaptation through 'this' must be first");
          return NULL;
        }
        break;
      }

      case TK_ID:
      {
        const char* name = ast_name(id);
        def = ast_get(scope, name);

        if(def == NULL)
        {
          ast_error(id, "can't find definition of '%s'", name);
          return NULL;
        }

        switch(ast_id(def))
        {
          case TK_PACKAGE:
          {
            if(scope != type_name)
            {
              ast_error(id, "only one package can appear in a type name");
              return NULL;
            }

            scope = def;
            break;
          }

          case TK_TYPE:
          case TK_TRAIT:
          case TK_CLASS:
          case TK_ACTOR:
          case TK_TYPEPARAM:
          {
            if(ast_sibling(id) != NULL)
            {
              ast_error(id, "a type must be the last element of a type name");
              return NULL;
            }

            ast_attach(nominal, def);
            return def;
          }

          case TK_FVAR:
          case TK_FVAL:
          case TK_IDSEQ:
          {
            // TODO: cap
            // TODO: change the scope to access the fields of the object
            if(scope != type_name)
            {
              ast_error(id,
                "viewpoint adaptation cannot occur after a package");
              return NULL;
            }
            break;
          }

          default:
          {
            ast_error(id, "only fields and local variables can be used for "
              "viewpoint adaptation");
            return NULL;
          }
        }
        break;
      }

      default: assert(0);
    }

    id = ast_sibling(id);
  }

  ast_error(type_name, "no type name present");
  return NULL;
}

#include "type.h"
#include "ponyassert.h"


static bool embed_struct_field_has_finaliser(pass_opt_t* opt, ast_t* field,
  ast_t* base)
{
  ast_t* type = ast_type(field);
  pony_assert(ast_id(type) == TK_NOMINAL);

  ast_t* def = (ast_t*)ast_data(type);

  ast_t* final = ast_get(def, stringtab("_final"), NULL);

  if(final != NULL)
  {
    ast_error(opt->check.errors, base,
      "a struct cannot have a field with a _final method");

    if(field != base)
      ast_error_continue(opt->check.errors, field, "nested field is here");

    ast_error_continue(opt->check.errors, final, "_final method is here");

    return false;
  }

  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  bool ok = true;

  while(member != NULL)
  {
    if(ast_id(member) == TK_EMBED)
    {
      if(!embed_struct_field_has_finaliser(opt, member, base))
        ok = false;
    }

    member = ast_sibling(member);
  }

  return ok;
}

bool verify_struct(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_STRUCT);

  ast_t* members = ast_childidx(ast, 4);
  ast_t* member = ast_child(members);

  bool ok = true;

  while(member != NULL)
  {
    if(ast_id(member) == TK_EMBED)
    {
      if(!embed_struct_field_has_finaliser(opt, member, member))
        ok = false;
    }

    member = ast_sibling(member);
  }

  return ok;
}

bool verify_interface(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_INTERFACE);

  bool ok = true;

  AST_GET_CHILDREN(ast, id, typeparams, defcap, provides, members, c_api);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FUN:
      case TK_BE:
      {
        AST_GET_CHILDREN(member, cap, id, type_params, params, return_type,
          error, body, docstring);

        const char* type_id_name = ast_name(id);

        if(is_name_private(type_id_name))
        {
          ast_error(opt->check.errors, id,
            "interfaces can't have private methods, only traits can");
          ok = false;
        }
      }
      default: {}
    }

    member = ast_sibling(member);
  }

  return ok;
}


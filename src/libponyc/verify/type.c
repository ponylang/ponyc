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

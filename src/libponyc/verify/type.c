#include "type.h"
#include "../type/typealias.h"
#include "ponyassert.h"


static bool embed_struct_field_has_finaliser(pass_opt_t* opt, ast_t* field,
  ast_t* base)
{
  ast_t* type = ast_type(field);

  // Embed fields with a typealias-typed declaration carry the
  // TK_TYPEALIASREF in ast_type; flatten validates that the unfolded
  // form is a class, but doesn't rewrite the field's type. Unfold
  // here to reach the concrete def.
  ast_t* unfolded = NULL;
  if(ast_id(type) == TK_TYPEALIASREF)
  {
    unfolded = typealias_unfold(type);
    if(unfolded != NULL)
      type = unfolded;
  }

  // The TK_NOMINAL assertion holds because flatten's TK_EMBED handling
  // (pass/flatten.c) rejects any embed field whose unfolded type isn't
  // a class — a struct-or-class with a known concrete def. If a
  // future flatten change ever leaves a non-nominal type past that
  // gate, the assertion fires here at the point of first use rather
  // than allowing a corrupt def_data lookup downstream.
  pony_assert(ast_id(type) == TK_NOMINAL);

  ast_t* def = (ast_t*)ast_data(type);

  ast_t* final = ast_get(def, stringtab(opt->strtab, "_final"), NULL);

  if(final != NULL)
  {
    ast_error(opt->check.errors, base,
      "a struct cannot have a field with a _final method");

    if(field != base)
      ast_error_continue(opt->check.errors, field, "nested field is here");

    ast_error_continue(opt->check.errors, final, "_final method is here");

    if(unfolded != NULL)
      ast_free_unattached(unfolded);
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

  if(unfolded != NULL)
    ast_free_unattached(unfolded);
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


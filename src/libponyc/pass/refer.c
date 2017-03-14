#include "refer.h"
#include "../expr/reference.h" // TODO: remove after moving def_before_use?
#include "../ast/id.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>


static const char* suggest_alt_name(ast_t* ast, const char* name)
{
  pony_assert(ast != NULL);
  pony_assert(name != NULL);

  size_t name_len = strlen(name);

  if(is_name_private(name))
  {
    // Try without leading underscore
    const char* try_name = stringtab(name + 1);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }
  else
  {
    // Try with a leading underscore
    char* buf = (char*)ponyint_pool_alloc_size(name_len + 2);
    buf[0] = '_';
    strncpy(buf + 1, name, name_len + 1);
    const char* try_name = stringtab_consume(buf, name_len + 2);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }

  // Try with a different case (without crossing type/value boundary)
  ast_t* case_ast = ast_get_case(ast, name, NULL);
  if(case_ast != NULL)
  {
    ast_t* id = case_ast;

    if(ast_id(id) != TK_ID)
      id = ast_child(id);

    pony_assert(ast_id(id) == TK_ID);
    const char* try_name = ast_name(id);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }

  // Give up
  return NULL;
}

bool refer_reference(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  const char* name = ast_name(ast_child(ast));

  // Handle the special case of the "don't care" reference (`_`)
  if(is_name_dontcare(name))
  {
    ast_setid(ast, TK_DONTCAREREF);

    return true;
  }

  // Everything we reference must be in scope, so we can use ast_get for lookup.
  ast_t* def = ast_get(ast, name, NULL);

  // If nothing was found, we fail, but also try to suggest an alternate name.
  if(def == NULL)
  {
    const char* alt_name = suggest_alt_name(ast, name);

    if(alt_name == NULL)
      ast_error(opt->check.errors, ast, "can't find declaration of '%s'", name);
    else
      ast_error(opt->check.errors, ast,
        "can't find declaration of '%s', did you mean '%s'?", name, alt_name);

    return false;
  }

  // Save the found definition in the AST, so we don't need to look it up again.
  ast_setdata(ast, (void*)def);

  switch(ast_id(def))
  {
    case TK_PACKAGE:
    {
      // Only allowed if in a TK_DOT with a type.
      if(ast_id(ast_parent(ast)) != TK_DOT)
      {
        ast_error(opt->check.errors, ast,
          "a package can only appear as a prefix to a type");
        return false;
      }

      ast_setid(ast, TK_PACKAGEREF);
      return true;
    }

    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_TYPE:
    case TK_TYPEPARAM:
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
    {
      ast_setid(ast, TK_TYPEREF);
      return true;
    }

    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    {
      // Transform to "this.f".
      if(!def_before_use(opt, def, ast, name))
        return false;

      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_child(ast));

      ast_t* self = ast_from(ast, TK_THIS);
      ast_add(dot, self);

      ast_replace(astp, dot);
      return true;
    }

    case TK_PARAM:
    {
      if(opt->check.frame->def_arg != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't reference a parameter in a default argument");
        return false;
      }

      if(!def_before_use(opt, def, ast, name))
        return false;

      ast_setid(ast, TK_PARAMREF);
      return true;
    }

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      // Transform to "this.f".
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_child(ast));

      ast_t* self = ast_from(ast, TK_THIS);
      ast_add(dot, self);

      ast_replace(astp, dot);
      return true;
    }

    case TK_ID: // TODO: consider changing where ast_set is done, so that this can be case TK_VAR, TK_LET, & TK_MATCH_CAPTURE
    {
      if(!def_before_use(opt, def, ast, name))
        return false;

      ast_t* var = ast_parent(def);

      switch(ast_id(var))
      {
        case TK_VAR:
          ast_setid(ast, TK_VARREF);
          break;

        case TK_LET:
        case TK_MATCH_CAPTURE:
          ast_setid(ast, TK_LETREF);
          break;

        default:
          pony_assert(0);
          return false;
      }

      return true;
    }

    default: {}
  }

  pony_assert(0);
  return false;
}

static bool package_access(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_DOT);

  // Left is a packageref, right is an id.
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  pony_assert(ast_id(left) == TK_PACKAGEREF);
  pony_assert(ast_id(right) == TK_ID);

  // Must be a type in a package.
  const char* package_name = ast_name(ast_child(left));
  ast_t* package = ast_get(left, package_name, NULL);

  if(package == NULL)
  {
    ast_error(opt->check.errors, right, "can't access package '%s'",
      package_name);
    return false;
  }

  pony_assert(ast_id(package) == TK_PACKAGE);
  const char* type_name = ast_name(right);
  ast_t* type = ast_get(package, type_name, NULL);

  if(type == NULL)
  {
    ast_error(opt->check.errors, right, "can't find type '%s' in package '%s'",
      type_name, package_name);
    return false;
  }

  ast_setdata(ast, (void*)type);
  ast_setid(ast, TK_TYPEREF);
  return true;
}

bool refer_dot(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_DOT);

  if(ast_id(ast_child(ast)) == TK_PACKAGEREF)
    return package_access(opt, ast);

  return true;
}

static bool qualify_typeref(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  ast_t* typeref = ast_child(ast);

  // If the typeref is already qualified, it can't be qualified again, so we'll
  // leave as TK_QUALIFY, so expr pass will sugar as qualified call to apply.
  if((ast_childcount(typeref) == 2) &&
    (ast_id(ast_childidx(typeref, 1)) == TK_TYPEARGS))
    return true;

  ast_t* def = (ast_t*)ast_data(typeref);
  pony_assert(def != NULL);

  // If the type isn't polymorphic it can't be qualified at all, so we'll
  // leave as TK_QUALIFY, so expr pass will sugar as qualified call to apply.
  ast_t* typeparams = ast_childidx(def, 1);
  if(ast_id(typeparams) == TK_NONE)
    return true;

  ast_setdata(ast, (void*)def);
  ast_setid(ast, TK_TYPEREF);
  return true;
}

bool refer_qualify(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_QUALIFY);

  if(ast_id(ast_child(ast)) == TK_TYPEREF)
    return qualify_typeref(opt, ast);

  return true;
}

ast_result_t pass_refer(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_REFERENCE: r = refer_reference(options, astp); break;
    case TK_DOT:       r = refer_dot(options, ast); break;
    case TK_QUALIFY:   r = refer_qualify(options, ast); break;
    default: {}
  }

  if(!r)
  {
    pony_assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}

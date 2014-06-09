#include "type_valid.h"
#include <stdio.h>
#include <assert.h>

ast_t* ast_from_type_name(ast_t* type_name)
{
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
        ast_t* ref = ast_get(scope, name);

        if(ref == NULL)
        {
          ast_error(id, "can't find definition of '%s'", name);
          return false;
        }

        switch(ast_id(ref))
        {
          case TK_PACKAGE:
          {
            if(scope != type_name)
            {
              ast_error(id, "only one package can appear in a type name");
              return NULL;
            }

            scope = ref;
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

            return ref;
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

bool attach_method(ast_t* type, ast_t* method)
{
  ast_t* impl = ast_childidx(method, 6);

  // if we aren't a trait and it has no implementation, we're done
  if((ast_id(type) != TK_TRAIT) && (ast_id(impl) == TK_NONE))
    return true;

  // copy the method ast
  ast_t* members = ast_childidx(type, 4);
  ast_t* method_dup = ast_dup(method);

  // TODO: substitute for any type parameters

  // see if we have an existing method with this name
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* existing = ast_get(type, name);

  if(existing != NULL)
  {
    // TODO: if we already have this method (by name):
    // check our version is a subtype of the supplied version
    assert(0);
  }

  // insert into our members
  ast_append(members, method_dup);

  // add to our scope
  ast_set(type, name, method_dup);

  return true;
}

bool attach_traits(ast_t* type)
{
  ast_t* traits = ast_childidx(type, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    // TODO: what if this trait hasn't had its own traits attached yet?
    ast_t* nominal = ast_child(trait);

    if(ast_id(nominal) != TK_NOMINAL)
    {
      ast_error(nominal, "traits must be nominal types");
      return false;
    }

    ast_t* type_name = ast_child(nominal);
    ast_t* trait_def = ast_from_type_name(type_name);

    if(ast_id(trait_def) != TK_TRAIT)
    {
      ast_error(nominal, "must be a trait");
      return false;
    }

    ast_t* members = ast_childidx(trait_def, 4);
    ast_t* member = ast_child(members);

    while(member != NULL)
    {
      switch(ast_id(member))
      {
        case TK_NEW:
        case TK_FUN:
        case TK_BE:
        {
          if(!attach_method(type, member))
            return false;
          break;
        }

        default: assert(0);
      }

      member = ast_sibling(member);
    }

    trait = ast_sibling(trait);
  }

  return true;
}

/**
 * This checks that all explicit types are valid.
 */
bool type_valid(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_TYPE:
      // TODO: constraints must be valid
      break;

    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // TODO: constraints must be valid
      if(!attach_traits(ast))
        return false;

      break;
    }

    case TK_NOMINAL:
    {
      // TODO: make sure typeargs are within constraints
      // typeargs can be nominal types as well
      // TODO: juggle name and cap
      // TODO: transform to a different ast node?
      ast_t* type_name = ast_child(ast);
      ast_t* ref = ast_from_type_name(type_name);

      if(ref == NULL)
        return false;

      ast_t* typeargs = ast_sibling(type_name);
      if(ast_id(typeargs) == TK_NONE)
        break;

      break;
    }

    case TK_ASSIGN:
      // TODO: check for syntactic sugar for update
      break;

    case TK_RECOVER:
      // TODO: remove things from following scope
      break;

    case TK_FOR:
      // TODO: syntactic sugar for a while loop
      break;

    default: {}
  }

  return true;
}

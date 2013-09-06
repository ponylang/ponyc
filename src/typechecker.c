#include "typechecker.h"
#include "package.h"
#include "types.h"
#include "error.h"

static bool resolve_type( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_ADT:
    case TK_OBJTYPE:
    case TK_FUNTYPE:
    {
      type_t* type = type_ast( ast );
      if( type == NULL ) { return false; }

      ast_attach( ast, type );
      return true;
    }

    default: {}
  }

  return true;
}

static bool set_scope( ast_t* scope, ast_t* name, ast_t* value, bool type )
{
  const char* s = ast_name( name );

  if( type )
  {
    if( (s[0] < 'A') || (s[0] > 'Z') )
    {
      ast_error( name, "type name '%s' must start A-Z", s );
      return false;
    }
  } else {
    if( (s[0] >= 'A') && (s[0] <= 'Z') )
    {
      ast_error( name, "identifier '%s' can't start A-Z", s );
      return false;
    }
  }

  if( !ast_set( scope, s, value ) )
  {
    ast_error( name, "can't reuse name '%s'", s );
    return false;
  }

  return true;
}

static bool use_package( ast_t* ast, const char* path, ast_t* name )
{
  ast_t* package = package_load( ast, path );

  if( package == NULL )
  {
    ast_error( ast, "can't load package '%s'", path );
    return false;
  }

  if( name && (ast_id( name ) == TK_ID) )
  {
    return set_scope( ast, name, package, false );
  }

  if( !ast_merge( ast, package ) )
  {
    ast_error( ast, "can't merge symbols from '%s'", path );
    return false;
  }

  return true;
}

static bool add_to_scope( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_USE:
    {
      ast_t* path = ast_child( ast );
      ast_t* name = ast_sibling( path );

      return use_package( ast, ast_name( path ), name );
    }

    case TK_ALIAS:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      return set_scope( ast_nearest( ast, TK_PACKAGE ),
        ast_child( ast ), ast, true );

    case TK_TYPEPARAM:
      return set_scope( ast, ast_child( ast ), ast, true );

    case TK_PARAM:
      return set_scope( ast, ast_child( ast ), ast, false );

    case TK_VAR:
    case TK_VAL:
    {
      ast_t* name = ast_child( ast );
      if( name == NULL ) { return true; }

      ast_t* trait = ast_nearest( ast, TK_TRAIT );

      if( trait != NULL )
      {
        ast_error( ast, "can't have field '%s' in trait '%s'",
          ast_name( name ), ast_name( ast_child( trait ) )
          );

        return false;
      }

      return set_scope( ast, name, ast, false );
    }

    case TK_FUN:
      return set_scope( ast, ast_childidx( ast, 2 ), ast, false );

    case TK_MSG:
    {
      ast_t* class = ast_nearest( ast, TK_CLASS );
      ast_t* name = ast_childidx( ast, 2 );

      if( class != NULL )
      {
        ast_error( ast, "can't have msg '%s' in class '%s'",
          ast_name( name ), ast_name( ast_child( class ) )
          );

        return false;
      }

      return set_scope( ast, name, ast, false );
    }

    default: {}
  }

  return true;
}

bool typecheck( ast_t* ast )
{
  bool ret = true;

  ret &= use_package( ast, "builtin", NULL );
  ret &= ast_visit( ast, add_to_scope, NULL );
  ret &= ast_visit( ast, resolve_type, NULL );

  /*
  FIX: check more things
  validate type parameters
    needs subtype relationship
    which needs objtype traits
    which need to be reified
    which needs to happen recursively for traits picked up through traits
    which needs to be flattened into the list of traits for the type
  */

  return ret;
}

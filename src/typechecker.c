#include "typechecker.h"
#include "types.h"
#include "error.h"

#define FOREACH(f) \
  { \
    ast_t* child = ast_child( ast ); \
    while( child != NULL ) \
    { \
      ret &= f( child ); \
      child = ast_sibling( child ); \
    } \
  }

static bool check_formals( ast_t* ast )
{
  if( ast_id( ast ) == TK_USE ) { return true; }

  ast_t* formals = ast_childidx( ast, 1 );
  ast_t* param = ast_child( formals );
  bool ret = true;

  while( param != NULL )
  {
    ast_t* param_type = ast_childidx( param, 1 );
    type_t* type = type_ast( param_type );

    if( type != NULL )
    {
      ast_attach( param_type, type );
      ast_attach( param, type );
    } else {
      ast_error( param, "can't resolve type '%s'",
        ast_name( ast_child( param ) ) );
      ret = false;
    }

    param = ast_sibling( param );
  }

  return ret;
}

static bool check_module( ast_t* ast )
{
  bool ret = true;
  FOREACH( check_formals );
  // FIX: check more things
  return ret;
}

static bool set( ast_t* scope, ast_t* id, ast_t* ast )
{
  const char* name = ast_name( id );

  if( !ast_set( scope, name, ast ) )
  {
    ast_error( id, "can't reuse name '%s'", name );
    return false;
  }

  return true;
}

static bool check_id( ast_t* ast, bool type )
{
  const char* name = ast_name( ast );

  if( type )
  {
    if( (name[0] < 'A') || (name[0] > 'Z') )
    {
      ast_error( ast, "type name '%s' must start A-Z", name );
      return false;
    }
  } else {
    if( (name[0] >= 'A') && (name[0] <= 'Z') )
    {
      ast_error( ast, "identifier '%s' can't start A-Z", name );
      return false;
    }
  }

  return true;
}

static bool prep_params( ast_t* ast, bool type )
{
  ast_t* scope = ast_parent( ast );
  ast_t* param = ast_child( ast );
  bool ret = true;

  while( param != NULL )
  {
    ast_t* id = ast_child( param );

    if( !check_id( id, type ) ) { ret = false; }
    if( !set( scope, id, param ) ) { ret = false; }

    param = ast_sibling( param );
  }

  return ret;
}

static bool prep_member( ast_t* ast )
{
  ast_t* class = ast_parent( ast );
  ast_t* id;

  switch( ast_id( ast ) )
  {
    case TK_VAR:
    case TK_VAL:
      id = ast_childidx( ast, 0 );

      if( ast_id( class ) == TK_TRAIT )
      {
        ast_error( ast, "can't have field '%s' in trait '%s'",
          ast_name( id ), ast_name( ast_child( class ) )
          );
        return false;
      }
      break;

    case TK_FUN:
      id = ast_childidx( ast, 2 );
      break;

    case TK_MSG:
      id = ast_childidx( ast, 2 );

      if( ast_id( class ) == TK_CLASS )
      {
        ast_error( ast, "can't have msg '%s' in class '%s'",
          ast_name( id ), ast_name( ast_child( class ) )
          );
        return false;
      }
      break;

    default:
      return true;
  }

  if( !check_id( id, false ) ) { return false; }
  if( !set( class, id, ast ) ) { return false; }

  switch( ast_id( ast ) )
  {
    case TK_FUN:
    case TK_MSG:
      return prep_params( ast_childidx( ast, 3 ), true )
        && prep_params( ast_childidx( ast, 5 ), false );

    default: {}
  }

  return true;
}

static bool prep_members( ast_t* ast )
{
  token_id id = ast_id( ast );
  if( (id == TK_ALIAS) || (id == TK_USE) ) { return true; }

  bool ret = true;
  FOREACH( prep_member );
  return ret;
}

static bool prep_formals( ast_t* ast )
{
  return (ast_id( ast ) == TK_USE)
    || prep_params( ast_childidx( ast, 1 ), true );
}

static bool prep_typename( ast_t* ast )
{
  if( ast_id( ast ) == TK_USE ) { return true; }

  ast_t* id = ast_child( ast );

  return check_id( id, true )
    && set( ast_nearest( ast, TK_PACKAGE ), id, ast );
}

static bool prep_module( ast_t* ast )
{
  bool ret = true;
  FOREACH( prep_typename );
  FOREACH( prep_formals );
  FOREACH( prep_members );
  return ret;
}

bool typecheck( ast_t* ast )
{
  if( ast_id( ast ) != TK_PACKAGE ) { return false; }

  bool ret = true;
  FOREACH( prep_module );
  FOREACH( check_module );
  return ret;
}

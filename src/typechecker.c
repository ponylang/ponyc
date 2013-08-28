#include "typechecker.h"
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

static bool check_module( ast_t* module )
{
  bool ret = true;
  // FIX:
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
      ast_error( ast, "type name must start A-Z" );
      return false;
    }
  } else {
    if( (name[0] >= 'A') && (name[0] <= 'Z') )
    {
      ast_error( ast, "identifier can't start A-Z" );
      return false;
    }
  }

  return true;
}

static bool prep_params( ast_t* scope, ast_t* ast, bool type )
{
  ast_t* child = ast_child( ast );
  bool ret = true;

  while( child != NULL )
  {
    if( !check_id( child, type ) ) { ret = false; }
    if( !set( scope, child, child ) ) { ret = false; }

    child = ast_sibling( child );
  }

  return ret;
}

static bool prep_member( ast_t* class, ast_t* ast )
{
  ast_t* id;

  switch( ast_id( ast ) )
  {
    case TK_VAR:
    case TK_VAL:
      if( ast_id( class ) == TK_TRAIT )
      {
        ast_error( ast, "can't have a field in a trait" );
        return false;
      }

      id = ast_childidx( ast, 0 );
      break;

    case TK_FUN:
      id = ast_childidx( ast, 1 );
      break;

    case TK_MSG:
      if( ast_id( class ) == TK_CLASS )
      {
        ast_error( ast, "can't have a msg in a class" );
        return false;
      }

      id = ast_childidx( ast, 1 );
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
      return prep_params( ast, ast_childidx( ast, 2 ), true )
        && prep_params( ast, ast_childidx( ast, 3 ), false );

    default: {}
  }

  return true;
}

static bool prep_class( ast_t* ast )
{
  bool ret = true;

  if( !prep_params( ast, ast_childidx( ast, 1 ), true ) ) { ret = false; }

  ast_t* child = ast_childidx( ast, 5 );

  while( child != NULL )
  {
    ret &= prep_member( ast, child );
    child = ast_sibling( child );
  }

  return ret;
}

static bool prep_type( ast_t* ast )
{
  ast_t* id = ast_child( ast );

  return check_id( id, true )
    && set( ast_nearest( ast, TK_PACKAGE ), id, ast );
}

static bool prep_module_child( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_ALIAS:
      return prep_type( ast );

    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      return prep_type( ast ) && prep_class( ast );

    default: break;
  }

  return true;
}

static bool prep_module( ast_t* ast )
{
  bool ret = true;
  FOREACH( prep_module_child );
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

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

  const char* name = ast_name( id );

  if( (name[0] >= 'A') && (name[0] <= 'Z') )
  {
    ast_error( id, "member name can't start A-Z" );
    return false;
  }

  if( !ast_set( class, name, ast ) )
  {
    ast_error( id, "can't reuse member name '%s'", name );
    return false;
  }

  return true;
}

static bool prep_class( ast_t* ast )
{
  ast_t* child = ast_childidx( ast, 5 );
  bool ret = true;

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
  const char* name = ast_name( id );

  if( (name[0] < 'A') || (name[0] > 'Z') )
  {
    ast_error( id, "type name must start A-Z" );
    return false;
  }

  if( !ast_set( ast_nearest( ast, TK_PACKAGE ), name, ast ) )
  {
    ast_error( id, "can't reuse type name '%s'", name );
    return false;
  }

  return true;
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

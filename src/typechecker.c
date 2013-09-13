#include "typechecker.h"
#include "stringtab.h"
#include "package.h"
#include "types.h"
#include "error.h"

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

static ast_t* gen_type( ast_t* ast )
{
  ast_t* obj = ast_newid( TK_OBJTYPE );
  token_t* t = token_new( TK_ID, 0, 0 );
  token_setstring( t, ast_name( ast_child( ast ) ) );
  ast_t* obj_id = ast_token( t );

  ast_t* list = ast_newid( TK_LIST );
  ast_t* param = ast_child( ast_childidx( ast, 1 ) );

  while( param != NULL )
  {
    ast_add( list, gen_type( param ) );
    param = ast_sibling( param );
  }

  ast_t* mode = ast_newid( TK_MODE );
  ast_add( mode, ast_newid( TK_LIST ) );
  ast_add( mode, ast_newid( TK_NONE ) );

  ast_add( obj, obj_id );
  ast_add( obj, ast_newid( TK_NONE ) );
  ast_add( obj, list );
  ast_add( obj, mode );

  return obj;
}

static void add_this_to_type( ast_t* ast )
{
  // FIX: crashing
  return;

  // FIX: if constrained on a trait, use the constraint
  ast_t* this = ast_new( TK_TYPEPARAM,
    ast_line( ast ), ast_pos( ast ), NULL );

  token_t* t = token_new( TK_ID, 0, 0 );
  token_setstring( t, "This" );
  ast_t* this_id = ast_token( t );

  ast_add( this, this_id );
  ast_add( this, gen_type( ast ) );
  ast_add( this, ast_newid( TK_NONE ) );

  ast_reverse( this );
  ast_append( ast, this );
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
      return set_scope( ast_nearest( ast, TK_PACKAGE ),
        ast_child( ast ), ast, true );

    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      add_this_to_type( ast );
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

static bool resolve_type( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_TYPEPARAM:
    case TK_PARAM:
    case TK_VAR:
    case TK_VAL:
    {
      // same type as child[1]
      ast_t* child = ast_childidx( ast, 1 );
      if( child != NULL ) { ast_attach( ast, ast_data( child ) ); }
      return true;
    }

    case TK_INFER:
    case TK_ADT:
    case TK_OBJTYPE:
    case TK_FUNTYPE:
    {
      type_t* type = type_ast( ast );
      if( type == NULL ) { return false; }
      ast_attach( ast, type );
      return true;
    }

    case TK_STRING:
      ast_attach( ast, type_name( ast, stringtab( "String" ) ) );
      return true;

    // FIX: other literals?

    case TK_NOT:
      /* FIX: unary, Bool or Integer */
      return true;

    case TK_MINUS:
      /* FIX: could be unary (Number) or binary (both the same number) */
      return true;

    case TK_AND:
    case TK_OR:
    case TK_XOR:
      /* FIX: both sides must either be Bool or the same Integer */
      return true;

    case TK_PLUS:
    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
      /* FIX: both sides must be the same Number */
      return true;

    case TK_LSHIFT:
    case TK_RSHIFT:
      /* FIX: left must be Integer, right Unsigned, result is the type of the
      left hand side
      */
      return true;

    case TK_EQ:
    case TK_NEQ:
    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
    {
      // right must be a subtype of left
      // left must be Comparable
      ast_t* left = ast_child( ast );
      ast_t* right = ast_sibling( left );

#if 0
      // FIX:
      if( !type_sub( ast_data( left ),
        type_name( ast, stringtab( "Comparable" ) ) )
        )
      {
        ast_error( ast,
          "left-hand side must be a subtype of Comparable" );
        return false;
      }
#endif

      if( !type_sub( ast_data( right ), ast_data( left ) ) )
      {
        ast_error( ast,
          "right-hand side must be a subtype of the left-hand side" );
        return false;
      }

      ast_attach( ast, type_name( ast, stringtab( "Bool" ) ) );
      return true;
    }

    default: {}
  }

  return true;
}

static bool check_type( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_ADT:
    case TK_OBJTYPE:
    case TK_FUNTYPE:
      return type_valid( ast, ast_data( ast ) );

    default: {}
  }

  return true;
}

bool typecheck( ast_t* ast )
{
  /*
  FIX: check more things
  modes
  objects can be functions based on the apply method
  expression typing
  */

  if( !use_package( ast, "builtin", NULL ) )
  {
    ast_error( ast, "couldn't load builtin" );
    return false;
  }

  if( !ast_visit( ast, add_to_scope, NULL ) )
  {
    ast_error( ast, "couldn't add to scope" );
    return false;
  }

  if( !ast_visit( ast, NULL, resolve_type ) )
  {
    ast_error( ast, "couldn't resolve types" );
    return false;
  }

  if( !ast_visit( ast, NULL, check_type ) )
  {
    ast_error( ast, "couldn't check types" );
    return false;
  }

  return true;
}

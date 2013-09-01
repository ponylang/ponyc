#include "typechecker.h"
#include "package.h"
#include "types.h"
#include "error.h"

static ast_ret resolve_type( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_ADT:
    case TK_OBJTYPE:
    case TK_FUNTYPE:
    {
      type_t* type = type_ast( ast );
      if( type == NULL ) { return AST_NEXT | AST_ERROR; }

      ast_attach( ast, type );
      return AST_NEXT;
    }

    default: {}
  }

  return AST_OK;
}

static ast_ret set_scope( ast_t* scope, ast_t* name, ast_t* value, bool type )
{
  const char* s = ast_name( name );

  if( type )
  {
    if( (s[0] < 'A') || (s[0] > 'Z') )
    {
      ast_error( name, "type name '%s' must start A-Z", s );
      return AST_ERROR;
    }
  } else {
    if( (s[0] >= 'A') && (s[0] <= 'Z') )
    {
      ast_error( name, "identifier '%s' can't start A-Z", s );
      return AST_ERROR;
    }
  }

  if( !ast_set( scope, s, value ) )
  {
    ast_error( name, "can't reuse name '%s'", s );
    return AST_ERROR;
  }

  return AST_OK;
}

static ast_ret add_to_scope( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_USE:
    {
      ast_t* path = ast_child( ast );
      ast_t* package = package_load( ast, ast_name( path ) );

      if( package == NULL )
      {
        ast_error( ast, "can't load package '%s'", ast_name( path ) );
        return AST_NEXT | AST_ERROR;
      }

      ast_t* name = ast_sibling( path );

      if( ast_id( name ) == TK_ID )
      {
        return AST_NEXT | set_scope( ast, name, package, false );
      }

      if( !ast_merge( ast, package ) )
      {
        ast_error( ast, "can't merge symbols from '%s'", ast_name( path ) );
        return AST_NEXT | AST_ERROR;
      }

      return AST_NEXT;
    }

    case TK_ALIAS:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      return set_scope( ast_nearest( ast, TK_PACKAGE ),
        ast_child( ast ), ast, true );

    case TK_TYPEPARAM:
      return AST_NEXT | set_scope( ast, ast_child( ast ), ast, true );

    case TK_PARAM:
      return AST_NEXT | set_scope( ast, ast_child( ast ), ast, false );

    case TK_VAR:
    case TK_VAL:
    {
      ast_t* name = ast_child( ast );
      if( name == NULL ) { return AST_NEXT; }

      ast_t* trait = ast_nearest( ast, TK_TRAIT );

      if( trait != NULL )
      {
        ast_error( ast, "can't have field '%s' in trait '%s'",
          ast_name( name ), ast_name( ast_child( trait ) )
          );

        return AST_NEXT | AST_ERROR;
      }

      return AST_NEXT | set_scope( ast, name, ast, false );
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

        return AST_NEXT | AST_ERROR;
      }

      return set_scope( ast, name, ast, false );
    }

    default: {}
  }

  return AST_OK;
}

bool typecheck( ast_t* ast )
{
  ast_ret error = AST_OK;

  error |= ast_visit( ast, add_to_scope );
  error |= ast_visit( ast, resolve_type );
  // FIX: check more things

  return (error & AST_ERROR) == 0;
}

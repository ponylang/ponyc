#include "typechecker.h"
#include "package.h"
#include "types.h"
#include "error.h"

static type_t* infer;
static type_t* none;
static type_t* boolean;
static type_t* string;
static type_t* integer;
static type_t* unsign;
static type_t* number;
static type_t* intlit;
static type_t* floatlit;
static type_t* numlit;
static type_t* comparable;
static type_t* ordered;

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

static ast_t* use_package( ast_t* ast, const char* path, ast_t* name )
{
  ast_t* package = package_load( ast, path );

  if( package == NULL )
  {
    ast_error( ast, "can't load package '%s'", path );
    return NULL;
  }

  if( name && (ast_id( name ) == TK_ID) )
  {
    if( set_scope( ast, name, package, false ) )
    {
      return package;
    } else {
      return NULL;
    }
  }

  if( !ast_merge( ast, package ) )
  {
    ast_error( ast, "can't merge symbols from '%s'", path );
    return NULL;
  }

  return package;
}

static ast_t* gen_type( ast_t* ast )
{
  ast_t* obj = ast_newid( TK_OBJTYPE );
  token_t* t = token_new( TK_ID, 0, 0 );
  token_setstring( t, ast_name( ast_child( ast ) ) );
  ast_t* obj_id = ast_token( t );

  ast_t* list = ast_newid( TK_LIST );

  if( ast_id( ast ) != TK_TYPEPARAM )
  {
    ast_t* param = ast_child( ast_childidx( ast, 1 ) );

    while( param != NULL )
    {
      ast_add( list, gen_type( param ) );
      param = ast_sibling( param );
    }
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
  // FIX: if constrained on a trait, use the constraint
  ast_t* this = ast_new( TK_TYPEPARAM,
    ast_line( ast ), ast_pos( ast ), NULL );

  token_t* t = token_new( TK_ID, 0, 0 );
  token_setstring( t, "This" );
  ast_t* this_id = ast_token( t );

  ast_add( this, this_id );
  ast_add( this, gen_type( ast ) );
  ast_add( this, ast_newid( TK_INFER ) );

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

      return use_package( ast, ast_name( path ), name ) != NULL;
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

    case TK_FIELD:
    {
      ast_t* name = ast_childidx( ast, 1 );
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
      return set_scope( ast_parent( ast ), ast_childidx( ast, 2 ), ast, false );

    case TK_MSG:
    {
      ast_t* name = ast_childidx( ast, 2 );
      ast_t* class = ast_nearest( ast, TK_CLASS );

      if( class != NULL )
      {
        ast_error( ast, "can't have msg '%s' in class '%s'",
          ast_name( name ), ast_name( ast_child( class ) )
          );

        return false;
      }

      return set_scope( ast_parent( ast ), name, ast, false );
    }

    case TK_LOCAL:
      return set_scope( ast, ast_childidx( ast, 1 ), ast, false );

    default: {}
  }

  return true;
}

static bool infer_type( ast_t* ast, size_t idx )
{
  ast_t* type_ast = ast_childidx( ast, idx );
  type_t* type = ast_data( type_ast );
  ast_attach( ast, type );

  ast_t* expr_ast = ast_sibling( type_ast );
  type_t* expr = ast_data( expr_ast );

  if( !type_sub( expr, type ) )
  {
    ast_error( ast, "the expression is not a subtype of the type" );
    return false;
  }

  if( ast_id( type_ast ) == TK_INFER ) { ast_attach( ast, expr ); }

  return true;
}

static bool same_number( ast_t* ast )
{
  ast_t* left = ast_child( ast );
  ast_t* right = ast_sibling( left );
  type_t* ltype = ast_data( left );
  type_t* rtype = ast_data( right );

  if( !type_sub( ltype, number ) )
  {
    ast_error( left, "must be a Number" );
    return false;
  }

  if( type_sub( ltype, numlit ) )
  {
    ast_attach( left, rtype );
    ast_attach( ast, rtype );
  } else if( type_sub( rtype, numlit ) ) {
    ast_attach( right, ltype );
    ast_attach( ast, ltype );
  } else if( type_eq( ltype, rtype ) ) {
    ast_attach( ast, ltype );
  } else {
    ast_error( left, "both sides must be the same Number type" );
    return false;
  }

  return true;
}

static bool trait_and_subtype( ast_t* ast, type_t* trait, const char* name )
{
  ast_t* left = ast_child( ast );
  ast_t* right = ast_sibling( left );
  type_t* ltype = ast_data( left );
  type_t* rtype = ast_data( right );

  if( !type_sub( ltype, trait ) )
  {
    ast_error( left, "must be %s", name );
    return false;
  }

  if( !type_sub( rtype, ltype ) )
  {
    ast_error( right, "must be a subtype of the left-hand side" );
    return false;
  }

  ast_attach( ast, boolean );
  return true;
}

static bool def_before_use( ast_t* def, ast_t* use )
{
  if( ast_id( def ) != TK_LOCAL ) { return true; }

  size_t line0 = ast_line( def );
  size_t pos0 = ast_pos( def );

  size_t line1 = ast_line( use );
  size_t pos1 = ast_pos( use );

  if( (line0 > line1)
    || ((line0 == line1) && (pos0 > pos1))
    )
  {
    ast_error( def, "identifier %s is used before it is defined",
      ast_name( use ) );
    return false;
  }

  return true;
}

static bool resolve_type( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_TYPEPARAM:
    case TK_PARAM:
      return infer_type( ast, 1 );

    case TK_FIELD:
      ast_attach( ast, ast_data( ast_childidx( ast, 2 ) ) );
      break;

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

    case TK_THIS:
      ast_attach( ast, type_name( ast, "This" ) );
      return true;

    case TK_STRING:
      ast_attach( ast, string );
      return true;

    case TK_INT:
      ast_attach( ast, intlit );
      return true;

    case TK_FLOAT:
      ast_attach( ast, floatlit );
      return true;

    case TK_REF:
    {
      ast_t* id = ast_child( ast );
      const char* name = ast_name( id );
      ast_t* def = ast_get( ast, name );

      // FIX: might be a Type
      if( def == NULL )
      {
        ast_error( id, "identifier %s is not in scope", name );
        return false;
      }

      // FIX:
      switch( ast_id( def ) )
      {
        case TK_LOCAL:
          if( !def_before_use( def, id ) ) { return false; }
          ast_attach( ast, ast_data( def ) );
          break;

        default: {}
      }

      return true;
    }

    case TK_SEQ:
    {
      // the type of a sequence is the type of the last expression
      size_t n = ast_childcount( ast );
      ast_t* last = ast_childidx( ast, n - 1 );
      ast_attach( ast, ast_data( last ) );
      return true;
    }

    case TK_DOT:
      // FIX:
      return true;

    case TK_TYPEARGS:
      // FIX:
      return true;

    case TK_CALL:
      // FIX:
      return true;

    case TK_NOT:
    {
      ast_t* child = ast_child( ast );
      type_t* type = ast_data( child );

      if( type_sub( type, boolean ) )
      {
        ast_attach( ast, boolean );
      } else if( type_sub( type, integer ) ) {
        ast_attach( ast, type );
      } else {
        ast_error( child, "must be a Bool or an Integer" );
        return false;
      }

      return true;
    }

    case TK_MINUS:
    {
      ast_t* left = ast_child( ast );
      ast_t* right = ast_sibling( left );
      type_t* ltype = ast_data( left );

      if( right != NULL )
      {
        return same_number( ast );
      } else if( type_sub( ltype, number ) ) {
        ast_attach( ast, ltype );
      } else {
        ast_error( left, "must be a Number" );
        return false;
      }

      return true;
    }

    case TK_AND:
    case TK_OR:
    case TK_XOR:
    {
      // both sides must either be Bool or the same Integer
      ast_t* left = ast_child( ast );
      ast_t* right = ast_sibling( left );
      type_t* ltype = ast_data( left );
      type_t* rtype = ast_data( right );

      if( type_sub( ltype, boolean ) )
      {
        if( type_sub( rtype, boolean ) )
        {
          ast_attach( ast, boolean );
        } else {
          ast_error( right, "must be a Bool" );
          return false;
        }
      } else if( type_sub( ltype, integer ) ) {
        if( type_sub( ltype, intlit ) )
        {
          ast_attach( left, rtype );
          ast_attach( ast, rtype );
        } else if( type_sub( rtype, intlit ) ) {
          ast_attach( right, ltype );
          ast_attach( ast, ltype );
        } else if( type_eq( ltype, rtype ) ) {
          ast_attach( ast, ltype );
        } else {
          ast_error( left, "both sides must be the same Integer type" );
          return false;
        }
      } else {
        ast_error( left, "must be a Bool or an Integer" );
        return false;
      }

      return true;
    }

    case TK_PLUS:
    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
      return same_number( ast );

    case TK_LSHIFT:
    case TK_RSHIFT:
    {
      // left must be Integer, right Unsigned, result is the type of left
      ast_t* left = ast_child( ast );
      ast_t* right = ast_sibling( left );
      type_t* ltype = ast_data( left );
      type_t* rtype = ast_data( right );

      if( !type_sub( ltype, integer ) )
      {
        ast_error( left, "must be an Integer" );
        return false;
      }

      if( !type_sub( rtype, unsign ) )
      {
        ast_error( right, "must be Unsigned" );
        return false;
      }

      ast_attach( ast, ltype );
      return true;
    }

    case TK_EQ:
    case TK_NEQ:
      return trait_and_subtype( ast, comparable, "Comparable" );

    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
      return trait_and_subtype( ast, ordered, "Ordered" );

    case TK_LOCAL:
      return infer_type( ast, 2 );

    case TK_IF:
      // FIX:
      return true;

    case TK_MATCH:
      // FIX:
      return true;

    case TK_CASE:
      // FIX:
      return true;

    case TK_AS:
      // FIX:
      return true;

    case TK_WHILE:
    {
      ast_t* cond = ast_child( ast );

      if( !type_sub( ast_data( cond ), boolean ) )
      {
        ast_error( cond, "while loop conditional must be a Bool" );
        return false;
      }

      ast_attach( ast, none );
      return true;
    }

    case TK_DO:
    {
      ast_t* cond = ast_childidx( ast, 1 );

      if( !type_sub( ast_data( cond ), boolean ) )
      {
        ast_error( cond, "do loop conditional must be a Bool" );
        return false;
      }

      ast_attach( ast, none );
      return true;
    }

    case TK_FOR:
      // FIX:
      ast_attach( ast, none );
      return true;

    case TK_TRY:
      /*
      FIX:
      type is finally type, if there is a finally clause
        otherwise, its the seq type and the seq type must match the catch type
      */
      return true;

    case TK_RETURN:
      // FIX:
      return true;

    case TK_BREAK:
    case TK_CONTINUE:
    {
      if( !ast_nearest( ast, TK_WHILE )
        && !ast_nearest( ast, TK_DO )
        && !ast_nearest( ast, TK_FOR )
        )
      {
        ast_error( ast, "%s can only appear in a loop", ast_name( ast ) );
        return false;
      }

      ast_attach( ast, none );
      return true;
    }

    case TK_THROW:
    {
      /*
      FIX: or in a function that throws
      disambiguate from the TK_THROW node in funtype, fun and lambda
       */
      if( !ast_nearest( ast, TK_TRY )
        )
      {
        ast_error( ast, "%s can only appear in a loop", ast_name( ast ) );
        return false;
      }

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

bool typecheck_init( ast_t* program )
{
  ast_t* builtin = package_load( program, "builtin" );

  if( builtin == NULL )
  {
//    ast_error( program, "couldn't load built-in types" );
    return false;
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
  ast_t* builtin = use_package( ast, "builtin", NULL );

  if( builtin == NULL )
  {
//    ast_error( ast, "couldn't use builtin" );
    return false;
  }

  if( !ast_visit( ast, add_to_scope, NULL ) )
  {
//    ast_error( ast, "couldn't add to scope" );
    return false;
  }

  if( infer == NULL )
  {
    infer = type_ast( NULL );
    none = type_name( builtin, "None" );
    boolean = type_name( builtin, "Bool" );
    string = type_name( builtin, "String" );
    integer = type_name( builtin, "Integer" );
    unsign = type_name( builtin, "Unsigned" );
    number = type_name( builtin, "Number" );
    intlit = type_name( builtin, "IntLiteral" );
    floatlit = type_name( builtin, "FloatLiteral" );
    numlit = type_name( builtin, "NumLiteral" );
    comparable = type_name( builtin, "Comparable" );
  }

  if( !ast_visit( ast, NULL, resolve_type ) )
  {
//    ast_error( ast, "couldn't resolve types" );
    return false;
  }

  if( !ast_visit( ast, NULL, check_type ) )
  {
//    ast_error( ast, "couldn't check types" );
    return false;
  }

  return true;
}

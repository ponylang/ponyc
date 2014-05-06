#include "types.h"
#include "hash.h"
#include "stringtab.h"
#include "list.h"
#include "table.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if 0

#define POINTER_ERROR ((void*)-1)

typedef enum
{
  M_ISO,
  M_TRN,
  M_VAR,
  M_VAL,
  M_BOX,
  M_TAG
} mode_id;

typedef enum
{
  T_INFER,
  T_FUNCTION,
  T_OBJECT,
  T_ADT
} type_id;

typedef enum
{
  T_UNCHECKED,
  T_CHECKING,
  T_VALID,
  T_INVALID
} type_ok;

typedef struct funtype_t
{
  list_t* params;
  type_t* result;
  bool throws;
} funtype_t;

typedef struct objtype_t
{
  ast_t* ast;
  list_t* params;
} objtype_t;

typedef struct adt_t
{
  list_t* types;
} adt_t;

struct type_t
{
  type_id id;
  mode_id mode;
  type_ok valid;
  // FIX: need mode viewpoint adaptation

  union
  {
    funtype_t fun;
    objtype_t obj;
    adt_t adt;
  };

  struct type_t* next;
};

static table_t* type_table;
static type_t infer = { T_INFER, 0 };

static type_t* type_subst( type_t* type, list_t* list );

static uint64_t type_hash( const void* in, uint64_t seed )
{
  const type_t* type = in;

  switch( type->id )
  {
    case T_INFER:
      break;

    case T_FUNCTION:
      seed = list_hash( type->fun.params, type_hash, seed );
      seed = type_hash( type->fun.result, seed );
      break;

    case T_OBJECT:
      seed = strhash( ast_name( ast_child( type->obj.ast ) ), seed );
      seed = list_hash( type->obj.params, type_hash, seed );
      break;

    case T_ADT:
      seed = list_hash( type->adt.types, type_hash, seed );
      break;
  }

  return seed;
}

static bool type_cmp( const void* a, const void* b )
{
  return type_eq( a, b );
}

static bool type_cmpsub( const void* a, const void* b )
{
      return type_sub( a, b );
}

static bool type_cmpsup( void* arg, void* iter )
{
  return type_sub( list_data( iter ), arg );
}

static void* type_dup( const void* data )
{
  return (void*)data;
}

static void type_free( void* m )
{
  type_t* type = m;
  if( type == NULL ) return;

  // don't free any other types, as they are in the table
  switch( type->id )
  {
    case T_INFER: return;

    case T_FUNCTION:
      list_free( type->fun.params, NULL );
      break;

    case T_OBJECT:
      list_free( type->obj.params, NULL );
      break;

    case T_ADT:
      list_free( type->adt.types, NULL );
      break;
  }

  free( type );
}

static type_t* type_new( type_id id )
{
  type_t* type = calloc( 1, sizeof(type_t) );
  type->id = id;
  return type;
}

static type_t* type_store( type_t* type )
{
  if( type_table == NULL )
    type_table = table_create( 4096, type_hash, type_cmp, type_dup, type_free );

  bool present;
  type_t* found = table_insert( type_table, type, &present );

  if( present ) type_free( type );
  return found;
}

static bool a_is_obj_b( const type_t* a, const type_t* b )
{
  // invariant formal parameters
  return (a->id == T_OBJECT)
    && (a->obj.ast == b->obj.ast)
    && list_equals( a->obj.params, b->obj.params, type_cmp );
}

static bool a_in_obj_b( const type_t* a, const type_t* b )
{
  if( a->id != T_OBJECT ) { return false; }
  if( a_is_obj_b( a, b ) ) { return true; }

  // check reified traits
  ast_t* trait = ast_child( ast_childidx( a->obj.ast, 4 ) );

  while( trait != NULL )
  {
    if( type_sub( type_subst( ast_data( trait ), a->obj.params ), b ) )
    {
      return true;
    }

    trait = ast_sibling( trait );
  }

  return false;
}

static bool a_is_fun_b( const type_t* a, const type_t* b )
{
  switch( a->id )
  {
    case T_FUNCTION:
    {
      // invariant parameters
      if( !list_equals( a->fun.params, b->fun.params, type_cmp ) ) return false;

      // invariant result
      if( !type_eq( a->fun.result, b->fun.result ) ) return false;

      // invariant throw
      if( a->fun.throws != b->fun.throws ) return false;

      return true;
    }

    default: {}
  }

  return false;
}

static bool a_in_fun_b( const type_t* a, const type_t* b )
{
  switch( a->id )
  {
    case T_FUNCTION:
    {
      // contravariant parameters
      if( !list_equals( b->fun.params, a->fun.params, type_cmpsub ) ) return false;

      // covariant result
      if( !type_sub( a->fun.result, b->fun.result ) ) { return false; }

      // a can't throw if b doesn't throw
      if( !b->fun.throws && a->fun.throws ) { return false; }

      return true;
    }

    case T_OBJECT:
      // FIX: see if the apply() method conforms
      return false;

    default: {}
  }

  return false;
}

static bool a_in_adt_b( const type_t* a, const type_t* b )
{
  return list_find( b->adt.types, type_cmpsub, a ) != NULL;
}

static bool adt_a_in_b( const type_t* a, const type_t* b )
{
  return list_test( a->adt.types, type_cmpsup, (void*)b );
}

static list_t* typelist( ast_t* ast )
{
  list_t* list = NULL;
  ast_t* child = ast_child( ast );

  while( child != NULL )
  {
    type_t* type = type_ast( child );

    if( type == NULL )
    {
      list_free( list, NULL );
      return POINTER_ERROR;
    }

    list = list_append( list, type );
    child = ast_sibling( child );
  }

  return list;
}

static type_t* expand_alias( type_t* type )
{
  if( ast_id( type->obj.ast ) != TK_ALIAS ) { return type; }

  type_t* alias = type_ast( ast_childidx( type->obj.ast, 2 ) );

  return type_subst( alias, type->obj.params );
}

static type_t* nametype( ast_t* scope, const char* name )
{
  ast_t* type_ast = ast_get( scope, name );

  if( type_ast == NULL )
  {
    if( !strcmp( name, "_" ) ) { return &infer; }
    return NULL;
  }

  type_t* type = type_new( T_OBJECT );
  type->obj.ast = type_ast;

  return type;
}

static type_t* objtype( ast_t* ast )
{
  ast_t* scope = ast_child( ast );
  ast_t* class = ast_sibling( scope );
  ast_t* param = ast_sibling( class );

  if( ast_id( class ) == TK_NONE )
  {
    class = scope;
    scope = ast;
  } else {
    ast_t* p = ast_get( ast_nearest( ast, TK_MODULE ), ast_name( scope ) );

    if( p == NULL )
    {
      ast_error( scope, "no package '%s' in scope", ast_name( scope ) );
      return NULL;
    }

    scope = p;
  }

  const char* name = ast_name( class );
  type_t* type = nametype( scope, name );

  if( (type == &infer) && (ast_child( param ) != NULL) )
  {
    ast_error( class, "type _ cannot have type arguments" );
    return NULL;
  }

  if( type == NULL )
  {
    ast_error( class, "no type '%s' in scope", name );
    return NULL;
  }

  list_t* list = typelist( param );

  if( list == POINTER_ERROR )
  {
    type_free( type );
    return NULL;
  }

  type->obj.params = list;

  // FIX: get the mode with the viewpoint

  return expand_alias( type );
}

static type_t* funtype( ast_t* ast )
{
  type_t* type = type_new( T_FUNCTION );
  ast_t* child = ast_child( ast );

  if( ast_id( child ) == TK_QUESTION ) { type->fun.throws = true; }
  child = ast_sibling( child );

  // FIX: get the mode with the viewpoint
  child = ast_sibling( child );
  list_t* list = typelist( child );

  if( list == POINTER_ERROR )
  {
    type_free( type );
    return NULL;
  }

  type->fun.params = list;

  child = ast_sibling( child );
  type->fun.result = type_ast( child );

  if( type->fun.result == NULL )
  {
    type_free( type );
    return NULL;
  }

  return type;
}

static type_t* adttype( ast_t* ast )
{
  type_t* type = type_new( T_ADT );
  list_t* list = typelist( ast );

  if( list == POINTER_ERROR )
  {
    type_free( type );
    return NULL;
  }

  type->adt.types = list;

  switch( list_length( type->adt.types ) )
  {
    case 0:
      // an ADT with no elements is an error
      ast_error( ast, "ADT is empty" );
      type_free( type );
      return NULL;

    case 1:
    {
      // if only one element, ditch the ADT wrapper
      type_t* child = list_data( type->adt.types );
      type_free( type );
      return child;
    }

    default: {}
  }

  return type;
}

static void* subst_map( void* map, void* iter )
{
  return type_subst( list_data( iter ), map );
}

static type_t* type_subst( type_t* type, list_t* list )
{
  type_t* subst;

  switch( type->id )
  {
    case T_INFER: return type;

    case T_FUNCTION:
      subst = type_new( T_FUNCTION );
      subst->fun.params = list_map( type->fun.params, subst_map, list );
      subst->fun.result = type_subst( type->fun.result, list );
      subst->fun.throws = type->fun.throws;
      break;

    case T_OBJECT:
      switch( ast_id( type->obj.ast ) )
      {
        case TK_ALIAS:
        case TK_TRAIT:
        case TK_CLASS:
        case TK_ACTOR:
          subst = type_new( T_OBJECT );
          subst->obj.ast = type->obj.ast;
          subst->obj.params = list_map( type->obj.params, subst_map, list );
          break;

        case TK_TYPEPARAM:
          subst = list_data( list_index( list, ast_index( type->obj.ast ) ) );
          break;

        default: return NULL;
      }
      break;

    case T_ADT:
      subst = type_new( T_ADT );
      subst->adt.types = list_map( type->adt.types, subst_map, list );
      break;

    default: subst = NULL;
  }

  return type_store( subst );
}

static bool arg_valid( ast_t* ast, type_t* type, type_t* arg, ast_t* param )
{
  if( arg->id == T_INFER ) { return true; }

  if( (arg->id == T_OBJECT)
    && (ast_id( arg->obj.ast ) == TK_TYPEPARAM )
    )
  {
    // if the argument is the parameter, it matches
    if( arg->obj.ast == param ) { return true; }
    arg = ast_data( arg->obj.ast );
  }

  // substitute the parameter list in the bounds
  type_t* subst = type_subst( ast_data( param ), type->obj.params );

  // check that the argument is a subtype of the resulting type
  if( !type_sub( arg, subst ) )
  {
    ast_error( ast, "type argument %zd isn't within the constraint",
      ast_index( param ) + 1 );
    return false;
  }

  return true;
}

static bool obj_valid( ast_t* ast, type_t* type )
{
  switch( ast_id( type->obj.ast ) )
  {
    case TK_ALIAS:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
    {
      ast_t* params = ast_childidx( type->obj.ast, 1 );
      int count_arg = list_length( type->obj.params );
      int count_param = ast_childcount( params );

      if( count_arg != count_param )
      {
        ast_error( ast, "type has %d type argument%s, expected %d",
          count_arg, (count_arg == 1) ? "" : "s", count_param );
        return false;
      }

      if( count_arg == 0 ) return true;

      list_t* arg = type->obj.params;
      ast_t* param = ast_child( params );

      while( arg != NULL )
      {
        if( !arg_valid( ast, type, list_data( arg ), param ) ) return false;
        arg = list_next( arg );
        param = ast_sibling( param );
      }

      return true;
    }

    case TK_TYPEPARAM:
    {
      if( type->obj.params != NULL )
      {
        ast_error( ast, "type parameter can't have type arguments" );
        return false;
      }

      return true;
    }

    default: {}
  }

  return false;
}

type_t* type_name( ast_t* ast, const char* name )
{
  name = stringtab( name );
  type_t* type = nametype( ast, name );

  if( type == NULL )
  {
    ast_error( ast, "no type '%s' in scope", name );
  }

  return type_store( expand_alias( type ) );
}

type_t* type_ast( ast_t* ast )
{
  if( ast == NULL ) { return &infer; }

  type_t* type;

  switch( ast_id( ast ) )
  {
    case TK_ADT:
      type = adttype( ast );
      break;

    case TK_FUNTYPE:
    case TK_LAMBDA:
      type = funtype( ast );
      break;

    case TK_OBJTYPE:
      type = objtype( ast );
      break;

    case TK_PARAM:
      type = ast_data( ast );
      break;

    case TK_INFER: return &infer;

    default: return NULL;
  }

  return type_store( type );
}

static bool valid_pred( void* ast, void* iter )
{
  return type_valid( ast, list_data( iter ) );
}

bool type_valid( ast_t* ast, type_t* type )
{
  if( type == NULL ) { return true; }

  switch( type->valid )
  {
    case T_CHECKING:
      ast_error( ast, "recursion while checking validity" );
      return false;

    case T_VALID: return true;
    case T_INVALID: return false;
    default: {}
  }

  type->valid = T_CHECKING;
  bool ret;

  switch( type->id )
  {
    case T_FUNCTION:
      ret = list_test( type->fun.params, valid_pred, ast )
        && obj_valid( ast, type->fun.result );
      break;

    case T_OBJECT: ret = obj_valid( ast, type ); break;
    case T_ADT: ret = list_test( type->adt.types, valid_pred, ast ); break;

    default: ret = true;
  }

  type->valid = ret ? T_VALID : T_INVALID;
  return ret;
}

bool type_eq( const type_t* a, const type_t* b )
{
  if( (a == b)
    || (a->id == T_INFER)
    || (b->id == T_INFER)
    )
  {
    return true;
  }

  if( a->id != b->id ) { return false; }

  switch( b->id )
  {
    case T_INFER: return true;
    case T_FUNCTION: return a_is_fun_b( a, b );
    case T_OBJECT: return a_is_obj_b( a, b );

    case T_ADT:
      return list_superset( a->adt.types, b->adt.types, type_cmp )
        && list_superset( b->adt.types, a->adt.types, type_cmp );
  }

  return false;
}

bool type_sub( const type_t* a, const type_t* b )
{
  if( (a == NULL) || (b == NULL) ) { return false; }

  if( (a == b)
    || (a->id == T_INFER)
    || (b->id == T_INFER)
    )
  {
    return true;
  }

  if( a->id == T_ADT ) { return adt_a_in_b( a, b ); }

  switch( b->id )
  {
    case T_INFER: return true;
    case T_FUNCTION: return a_in_fun_b( a, b );
    case T_OBJECT: return a_in_obj_b( a, b );
    case T_ADT: return a_in_adt_b( a, b );
  }

  return false;
}

static bool print_pred( void* arg, void* iter )
{
  type_print( list_data( iter ) );
  if( list_next( iter ) != NULL ) printf( "%s", (const char*)arg );
  return true;
}

void type_print( const type_t* a )
{
  if( a == NULL )
  {
    printf( "null" );
    return;
  }

  switch( a->id )
  {
    case T_INFER:
      printf( "infer" );
      break;

    case T_FUNCTION:
      printf( "fun" );
      if( a->fun.throws ) { printf( " throw " ); }
      printf( "(" );
      list_test( a->fun.params, print_pred, ", " );
      printf( "):" );
      type_print( a->fun.result );
      break;

    case T_OBJECT:
      printf( "%s", ast_name( ast_child( a->obj.ast ) ) );

      if( a->obj.params != NULL )
      {
        printf( "[" );
        list_test( a->obj.params, print_pred, ", " );
        printf( "]" );
      }
      break;

    case T_ADT:
      printf( "(" );
      list_test( a->adt.types, print_pred, "|" );
      printf( ")" );
      break;
  }
}

#endif

void type_done()
{
#if 0
  table_free( type_table );
#endif
}

#include "types.h"
#include "hash.h"
#include "stringtab.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HASH_SIZE 4096
#define HASH_MASK (HASH_SIZE - 1)

typedef enum
{
  M_ISO = (1 << 0),
  M_VAR = (1 << 1),
  M_VAL = (1 << 2),
  M_TAG = (1 << 3)
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

typedef struct typelist_t
{
  type_t* type;
  struct typelist_t* next;
} typelist_t;

typedef struct funtype_t
{
  typelist_t* params;
  type_t* result;
  bool throws;
} funtype_t;

typedef struct objtype_t
{
  ast_t* ast;
  typelist_t* params;
} objtype_t;

typedef struct adt_t
{
  typelist_t* types;
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

typedef struct typetab_t
{
  typelist_t* type[HASH_SIZE];
} typetab_t;

static typetab_t table;
static type_t infer = { T_INFER, 0 };

static void type_free( type_t* type );
static uint64_t type_hash( type_t* type, uint64_t seed );
static type_t* type_subst( type_t* type, typelist_t* list );

static void typelist_free( typelist_t* list )
{
  typelist_t* next;

  while( list != NULL )
  {
    // don't free the component types, as they are in the table
    next = list->next;
    free( list );
    list = next;
  }
}

static void type_free( type_t* type )
{
  if( type == NULL ) { return; }

  // don't free any other types, as they are in the table
  switch( type->id )
  {
    case T_INFER: return;

    case T_FUNCTION:
      typelist_free( type->fun.params );
      break;

    case T_OBJECT:
      typelist_free( type->obj.params );
      break;

    case T_ADT:
      typelist_free( type->adt.types );
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

static uint64_t typelist_hash( typelist_t* list, uint64_t seed )
{
  while( list != NULL )
  {
    seed = type_hash( list->type, seed );
    list = list->next;
  }

  return seed;
}

static uint64_t type_hash( type_t* type, uint64_t seed )
{
  switch( type->id )
  {
    case T_INFER:
      break;

    case T_FUNCTION:
      seed = typelist_hash( type->fun.params, seed );
      seed = type_hash( type->fun.result, seed );
      break;

    case T_OBJECT:
      seed = strhash( ast_name( ast_child( type->obj.ast ) ), seed );
      seed = typelist_hash( type->obj.params, seed );
      break;

    case T_ADT:
      seed = typelist_hash( type->adt.types, seed );
      break;
  }

  return seed;
}

static type_t* typetable( type_t* type )
{
  if( type == NULL ) { return NULL; }

  uint64_t hash = type_hash( type, 0 ) & HASH_MASK;
  typelist_t* list = table.type[hash];

  while( list != NULL )
  {
    if( list->type == type ) { return type; }

    if( type_eq( list->type, type ) )
    {
      type_free( type );
      return list->type;
    }

    list = list->next;
  }

  list = malloc( sizeof(typelist_t) );
  list->type = type;
  list->next = table.type[hash];
  table.type[hash] = list;

  return type;
}

static bool typelist_has( typelist_t* list, type_t* type )
{
  // the type appears in the list
  while( list != NULL )
  {
    if( type_eq( type, list->type ) ) { return true; }
    list = list->next;
  }

  return false;
}

static bool typelist_contains( typelist_t* list, typelist_t* sub )
{
  // every element in sub must be equal to some element in list
  while( sub != NULL )
  {
    if( !typelist_has( list, sub->type ) ) { return false; }
    sub = sub->next;
  }

  return true;
}

static bool typelist_eq( typelist_t* a, typelist_t* b )
{
  // every element is equal, evaluated in order
  while( a != NULL )
  {
    if( (b == NULL) || !type_eq( a->type, b->type ) ) { return false; }
    a = a->next;
    b = b->next;
  }

  return b == NULL;
}

static bool typelist_sub( typelist_t* a, typelist_t* b )
{
  // every element of a is a subtype of the same element in b
  // evaluated in order
  while( a != NULL )
  {
    if( (b == NULL) || !type_eq( a->type, b->type ) ) { return false; }
    a = a->next;
    b = b->next;
  }

  return b == NULL;
}

static bool a_is_obj_b( type_t* a, type_t* b )
{
  // invariant formal parameters
  return (a->id == T_OBJECT)
    && (a->obj.ast == b->obj.ast)
    && typelist_eq( a->obj.params, b->obj.params );
}

static bool a_in_obj_b( type_t* a, type_t* b )
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

  if( (ast_id( b->obj.ast ) == TK_TRAIT)
    && (ast_id( ast_childidx( b->obj.ast, 3 ) ) == TK_INFER)
    )
  {
    // FIX: if b is an inferred trait, check for conformance
  }

  return false;
}

static bool a_is_fun_b( type_t* a, type_t* b )
{
  switch( a->id )
  {
    case T_FUNCTION:
    {
      // invariant parameters
      if( !typelist_eq( a->fun.params, b->fun.params ) ) { return false; }

      // invariant result
      if( !type_eq( a->fun.result, b->fun.result ) ) { return false; }

      // invariant throw
      if( a->fun.throws != b->fun.throws ) { return false; }

      return true;
    }

    default: {}
  }

  return false;
}

static bool a_in_fun_b( type_t* a, type_t* b )
{
  switch( a->id )
  {
    case T_FUNCTION:
    {
      // contravariant parameters
      if( !typelist_sub( b->fun.params, a->fun.params ) ) { return false; }

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

static bool a_in_adt_b( type_t* a, type_t* b )
{
  typelist_t* c = b->adt.types;

  while( c != NULL )
  {
    if( type_sub( a, c->type ) ) { return true; }
    c = c->next;
  }

  return false;
}

static bool adt_a_in_b( type_t* a, type_t* b )
{
  typelist_t* c = a->adt.types;

  while( c != NULL )
  {
    if( !type_sub( c->type, b ) ) { return false; }
    c = c->next;
  }

  return true;
}

static int typelist_len( typelist_t* list )
{
  int len = 0;

  while( list != NULL )
  {
    len++;
    list = list->next;
  }

  return len;
}

static typelist_t** typelist_add( typelist_t** list, type_t* type )
{
  typelist_t* node = malloc( sizeof(typelist_t) );
  node->type = type;
  node->next = NULL;
  *list = node;
  return &node->next;
}

static bool typelist( ast_t* ast, typelist_t** list )
{
  ast_t* child = ast_child( ast );

  while( child != NULL )
  {
    type_t* type = type_ast( child );
    if( type == NULL ) { return false; }
    list = typelist_add( list, type );
    child = ast_sibling( child );
  }

  return true;
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

  if( !typelist( param, &type->obj.params ) )
  {
    type_free( type );
    return NULL;
  }

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

  if( !typelist( child, &type->fun.params ) )
  {
    type_free( type );
    return NULL;
  }

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

  if( !typelist( ast, &type->adt.types ) )
  {
    type_free( type );
    return NULL;
  }

  switch( typelist_len( type->adt.types ) )
  {
    case 0:
      // an ADT with no elements is an error
      ast_error( ast, "ADT is empty" );
      type_free( type );
      return NULL;

    case 1:
    {
      // if only one element, ditch the ADT wrapper
      type_t* child = type->adt.types->type;
      type->adt.types->type = NULL;
      type_free( type );
      return child;
    }

    default: {}
  }

  return type;
}

static void typelist_subst( typelist_t** to, typelist_t* from, typelist_t* list )
{
  while( from != NULL )
  {
    to = typelist_add( to, type_subst( from->type, list ) );
    from = from->next;
  }
}

static type_t* type_subst( type_t* type, typelist_t* list )
{
  type_t* subst;

  switch( type->id )
  {
    case T_INFER: return type;

    case T_FUNCTION:
      subst = type_new( T_FUNCTION );
      typelist_subst( &subst->fun.params, type->fun.params, list );
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
          typelist_subst( &subst->obj.params, type->obj.params, list );
          break;

        case TK_TYPEPARAM:
        {
          int idx = ast_index( type->obj.ast );
          typelist_t* sub = list;

          while( idx > 0 )
          {
            sub = sub->next;
            idx--;
          }

          subst = sub->type;
          break;
        }

        default: return NULL;
      }
      break;

    case T_ADT:
      subst = type_new( T_ADT );
      typelist_subst( &subst->adt.types, type->adt.types, list );
      break;

    default: subst = NULL;
  }

  return typetable( subst );
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
      int count_arg = typelist_len( type->obj.params );
      int count_param = ast_childcount( params );

      if( count_arg != count_param )
      {
        ast_error( ast, "type has %d type argument%s, expected %d",
          count_arg, (count_arg == 1) ? "" : "s", count_param );
        return false;
      }

      if( count_arg == 0 ) { return true; }

      typelist_t* arg = type->obj.params;
      ast_t* param = ast_child( params );

      while( arg != NULL )
      {
        if( !arg_valid( ast, type, arg->type, param ) ) { return false; }
        arg = arg->next;
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

static bool typelist_valid( ast_t* ast, typelist_t* list )
{
  while( list != NULL )
  {
    if( !type_valid( ast, list->type ) ) { return false; }
    list = list->next;
  }

  return true;
}

type_t* type_name( ast_t* ast, const char* name )
{
  name = stringtab( name );
  type_t* type = nametype( ast, name );

  if( type == NULL )
  {
    ast_error( ast, "no type '%s' in scope", name );
  }

  return typetable( expand_alias( type ) );
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

  return typetable( type );
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
      ret = typelist_valid( ast, type->fun.params )
        && obj_valid( ast, type->fun.result );
      break;

    case T_OBJECT: ret = obj_valid( ast, type ); break;
    case T_ADT: ret = typelist_valid( ast, type->adt.types ); break;

    default: ret = true;
  }

  type->valid = ret ? T_VALID : T_INVALID;
  return ret;
}

bool type_eq( type_t* a, type_t* b )
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
      return typelist_contains( a->adt.types, b->adt.types )
        && typelist_contains( b->adt.types, a->adt.types );
  }

  return false;
}

bool type_sub( type_t* a, type_t* b )
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

static void typelist_print( typelist_t* list, const char* sep )
{
  while( list != NULL )
  {
    type_print( list->type );
    list = list->next;
    if( list != NULL ) { printf( "%s", sep ); }
  }
}

void type_print( type_t* a )
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
      typelist_print( a->fun.params, ", " );
      printf( "):" );
      type_print( a->fun.result );
      break;

    case T_OBJECT:
      printf( "%s", ast_name( ast_child( a->obj.ast ) ) );

      if( a->obj.params != NULL )
      {
        printf( "[" );
        typelist_print( a->obj.params, ", " );
        printf( "]" );
      }
      break;

    case T_ADT:
      printf( "(" );
      typelist_print( a->adt.types, "|" );
      printf( ")" );
      break;
  }
}

void type_done()
{
  typelist_t* list;
  typelist_t* next;

  // don't use typelist_free: need to actually free the types here
  for( int i = 0; i < HASH_SIZE; i++ )
  {
    list = table.type[i];

    while( list != NULL )
    {
      next = list->next;
      type_free( list->type );
      free( list );
      list = next;
    }
  }

  memset( table.type, 0, HASH_SIZE * sizeof(typelist_t*) );
}

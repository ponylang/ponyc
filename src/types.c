#include "types.h"
#include "hash.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

  // FIX: not right, need reified traits
  ast_t* is = ast_childidx( a->obj.ast, 4 );
  ast_t* trait = ast_child( is );

  while( trait != NULL )
  {
    if( a_in_obj_b( ast_data( trait ), b ) ) { return true; }
    trait = ast_sibling( trait );
  }

  // FIX: if b is an infered trait, check for conformance
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

static bool typelist( ast_t* ast, typelist_t** list )
{
  typelist_t* node;
  type_t* type;

  while( ast != NULL )
  {
    type = type_ast( ast );
    if( type == NULL ) { return false; }

    node = malloc( sizeof(typelist_t) );
    node->type = type;
    node->next = NULL;

    *list = node;
    list = &node->next;

    ast = ast_sibling( ast );
  }

  return true;
}

static type_t* objtype( ast_t* ast )
{
  type_t* type = calloc( 1, sizeof(type_t) );
  type->id = T_OBJECT;

  ast_t* package = ast_child( ast );
  ast_t* class = ast_sibling( package );
  ast_t* param = ast_sibling( class );

  if( ast_id( class ) == TK_NONE )
  {
    class = package;
    package = NULL;
  }

  if( package != NULL )
  {
    ast_t* p = ast_get( ast_nearest( ast, TK_MODULE ), ast_name( package ) );

    if( p == NULL )
    {
      ast_error( package, "no package '%s' in scope", ast_name( package ) );
      type_free( type );
      return NULL;
    }

    package = p;
  } else {
    package = ast;
  }

  type->obj.ast = ast_get( package, ast_name( class ) );

  if( type->obj.ast == NULL )
  {
    ast_error( class, "no type '%s' in scope", ast_name( class ) );
    type_free( type );
    return NULL;
  }

  if( !typelist( ast_child( param ), &type->obj.params ) )
  {
    type_free( type );
    return NULL;
  }

  // FIX: check params are valid

  // FIX: get the mode with the viewpoint

  // FIX: need to reify any traits that have formal parameters

  return type;
}

static type_t* funtype( ast_t* ast )
{
  type_t* type = calloc( 1, sizeof(type_t) );
  type->id = T_FUNCTION;

  ast_t* child = ast_child( ast );

  if( ast_id( child ) == TK_THROW ) { type->fun.throws = true; }
  child = ast_sibling( child );

  // FIX: get the mode with the viewpoint
  child = ast_sibling( child );

  if( !typelist( ast_child( child ), &type->fun.params ) )
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
  type_t* type = calloc( 1, sizeof(type_t) );
  type->id = T_ADT;

  if( !typelist( ast_child( ast ), &type->adt.types ) )
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

type_t* type_ast( ast_t* ast )
{
  type_t* type;

  switch( ast_id( ast ) )
  {
    case TK_ADT:
      type = adttype( ast );
      break;

    case TK_FUNTYPE:
      type = funtype( ast );
      break;

    case TK_OBJTYPE:
      type = objtype( ast );
      break;

    case TK_INFER:
      return &infer;

    default:
      return NULL;
  }

  return typetable( type );
}

bool type_eq( type_t* a, type_t* b )
{
  if( a == b ) { return true; }
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
  if( a == b ) { return true; }
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

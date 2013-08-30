#include "types.h"
#include <stdlib.h>

static type_t infer = { T_INFER, 0 };

static bool a_in_obj_b( type_t* a, type_t* b )
{
  if( a->id != T_OBJECT ) { return false; }

  if( a->obj.ast == b->obj.ast )
  {
    // FIX: check that formal params match
    return true;
  }

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

static bool a_in_fun_b( type_t* a, type_t* b )
{
  switch( a->id )
  {
    case T_FUNCTION:
    {
      // contravariant parameters
      typelist_t* pa = a->fun.params;
      typelist_t* pb = b->fun.params;

      while( pa != NULL )
      {
        if( (pb == NULL) || !type_sub( pb->type, pa->type ) ) { return false; }
        pa = pa->next;
        pb = pb->next;
      }

      if( pb != NULL ) { return false; }

      // covariant result
      if( !type_sub( a->fun.result, b->fun.result ) ) { return false; }

      // a can't throw if b doesn't throw
      if( !b->fun.throws && a->fun.throws ) { return false; }

      return true;
    }

    case T_OBJECT:
      // FIX: see if the apply() method conforms
      return false;

    default:
      return false;
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

static bool typelist( ast_t* ast, typelist_t** list )
{
  typelist_t* node;
  type_t* type;

  while( ast != NULL )
  {
    type = type_ast( ast );
    if( type == NULL ) { return false; }

    node = calloc( 1, sizeof(typelist_t) );
    node->type = type;

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
    package = ast_get( ast_nearest( ast, TK_MODULE ), ast_name( package ) );
  } else {
    package = ast;
  }

  if( package == NULL )
  {
    type_free( type );
    return NULL;
  }

  class = ast_get( package, ast_name( class ) );

  if( class == NULL )
  {
    type_free( type );
    return NULL;
  }

  type->obj.ast = class;

  if( !typelist( ast_child( param ), &type->obj.params ) )
  {
    type_free( type );
    return NULL;
  }

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

  return type;
}

static void typelist_free( typelist_t* list )
{
  typelist_t* next;

  while( list != NULL )
  {
    next = list->next;
    type_free( list->type );
    free( list );
    list = next;
  }
}

void type_init()
{
  // FIX: keep a table of types
}

void type_done()
{
  // FIX: clean up the table of types
}

type_t* type_ast( ast_t* ast )
{
  switch( ast_id( ast ) )
  {
    case TK_ADT: return adttype( ast );
    case TK_FUNTYPE: return funtype( ast );
    case TK_OBJTYPE: return objtype( ast );
    case TK_INFER: return &infer;
    default: return NULL;
  }
}

void type_free( type_t* type )
{
  if( type == NULL ) { return; }

  switch( type->id )
  {
    case T_INFER: return;

    case T_FUNCTION:
      typelist_free( type->fun.params );
      type_free( type->fun.result );
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

#include "types.h"

static bool a_in_obj_b( type_t* a, type_t* b )
{
  if( a->id != T_OBJECT ) { return false; }

  typelist_t* is = a->obj.is;

  while( is != NULL )
  {
    if( a_in_obj_b( is->type, b ) ) { return true; }
    is = is->next;
  }

  // FIX: if b is an inferred trait, check for conformance
  return false;
}

static bool a_in_fun_b( type_t* a, type_t* b )
{
  switch( a->id )
  {
    case T_FUNCTION:
    {
      // contravariant parameters
      if( a->fun.param_count != b->fun.param_count ) { return false; }
      typelist_t* pa = a->fun.params;
      typelist_t* pb = b->fun.params;

      while( pa != NULL )
      {
        if( !type_sub( pb->type, pa->type ) ) { return false; }
        pa = pa->next;
        pb = pb->next;
      }

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

void type_init()
{

}

void type_done()
{

}

type_t* type_ast( ast_t* ast )
{
  // FIX:
  return NULL;
}

bool type_sub( type_t* a, type_t* b )
{
  if( a == b ) { return true; }

  if( a->id == T_ADT ) { return adt_a_in_b( a, b ); }

  switch( b->id )
  {
    case T_INFER:
      return true;

    case T_FUNCTION:
      return a_in_fun_b( a, b );

    case T_OBJECT:
      return a_in_obj_b( a, b );

    case T_ADT:
      return a_in_adt_b( a, b );

    default:
      return a->id == b->id;
  }

  return false;
}

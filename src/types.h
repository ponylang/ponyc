#ifndef TYPES_H
#define TYPES_H

#include "ast.h"
#include <stdint.h>
#include <stdbool.h>

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

typedef struct type_t type_t;

typedef struct typelist_t
{
  type_t* type;
  struct typelist_t* next;
} typelist_t;

typedef struct funtype_t
{
  uint64_t param_count;
  typelist_t* params;
  type_t* result;
  bool throws;
} funtype_t;

typedef struct objtype_t
{
  ast_t* ast;
  typelist_t* is;
  typelist_t* functions;
  bool infer;
} objtype_t;

typedef struct adt_t
{
  typelist_t* types;
} adt_t;

struct type_t
{
  type_id id;
  mode_id mode;

  union
  {
    funtype_t fun;
    objtype_t obj;
    adt_t adt;
  };

  struct type_t* next;
};

void type_init();
void type_done();

type_t* type_ast( ast_t* ast );
bool type_sub( type_t* a, type_t* b );

#endif

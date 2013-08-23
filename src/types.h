#ifndef TYPES_H
#define TYPES_H

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
  T_NONE,
  T_BOOL,
  T_I8,
  T_U8,
  T_I16,
  T_U16,
  T_I32,
  T_U32,
  T_I64,
  T_U64,
  T_I128,
  T_U128,
  T_F32,
  T_F64,
  T_INT,
  T_FLOAT,
  T_STRING,
  T_FUNCTION,
  T_CLASS,
  T_PARTIAL,
  T_ADT
} type_id;

typedef enum
{
  O_TRAIT,
  O_CLASS,
  O_ACTOR
} class_id;

typedef struct type_t type_t;

typedef struct typelist_t
{
  type_t* type;
  struct typelist_t* next;
} typelist_t;

typedef struct functiontype_t
{
  typelist_t* params;
  type_t* result;
} functiontype_t;

typedef struct classdef_t
{
  class_id id;
//  symtab_t* symtab;

  struct classdef_t* next;
} classdef_t;

typedef struct classtype_t
{
  classdef_t* def;
  typelist_t* formal;
} classtype_t;

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
    functiontype_t fun;
    classtype_t obj;
    adt_t adt;
  };

  struct type_t* next;
};

#endif

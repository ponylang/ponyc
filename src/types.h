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
  T_UNKNOWN,
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
  typelist_t* formals;
  typelist_t* params;
  type_t* result;
  bool private;
  bool throws;
} functiontype_t;

typedef struct objtype_t
{
  ast_t* ast;
  typelist_t* formals;
  typelist_t* is;
  bool private;
  bool infer;
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
    funtype_t fun;
    objtype_t obj;
    adt_t adt;
  };

  struct type_t* next;
};

#endif

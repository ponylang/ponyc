#ifndef ds_stack_h
#define ds_stack_h

#include <platform.h>

PONY_EXTERN_C_BEGIN

#define STACK_COUNT 62

typedef struct Stack
{
  int index;
  void* data[STACK_COUNT];
  struct Stack* prev;
} Stack;

Stack* ponyint_stack_pop(Stack* stack, void** data);

Stack* ponyint_stack_push(Stack* list, void* data);

#define DECLARE_STACK(name, name_t, elem) \
  typedef struct name_t name_t; \
  name_t* name##_pop(name_t* stack, elem** data); \
  name_t* name##_push(name_t* stack, elem* data); \

#define DEFINE_STACK(name, name_t, elem) \
  struct name_t {}; \
  \
  name_t* name##_pop(name_t* stack, elem** data) \
  { \
    return (name_t*)ponyint_stack_pop((Stack*)stack, (void**)data); \
  } \
  name_t* name##_push(name_t* stack, elem* data) \
  { \
    return (name_t*)ponyint_stack_push((Stack*)stack, data); \
  } \

PONY_EXTERN_C_END

#endif

#ifndef ds_stack_h
#define ds_stack_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Stack Stack;

Stack* stack_pop(Stack* stack, void** data);

Stack* stack_push(Stack* list, void* data);

#define DECLARE_STACK(name, elem) \
  typedef struct name##_t name##_t; \
  name##_t* name##_pop(name##_t* stack, elem** data); \
  name##_t* name##_push(name##_t* stack, elem* data); \

#define DEFINE_STACK(name, elem) \
  struct name##_t {}; \
  \
  name##_t* name##_pop(name##_t* stack, elem** data) \
  { \
    return (name##_t*)stack_pop((Stack*)stack, (void**)data); \
  } \
  name##_t* name##_push(name##_t* stack, elem* data) \
  { \
    return (name##_t*)stack_push((Stack*)stack, data); \
  } \

#ifdef __cplusplus
}
#endif

#endif

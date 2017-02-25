#include "stack.h"
#include "../mem/pool.h"
#include "ponyassert.h"

static Stack* stack_new(Stack* prev, void* data)
{
  Stack* stack = (Stack*)POOL_ALLOC(Stack);
  stack->index = 1;
  stack->data[0] = data;
  stack->prev = prev;

  return stack;
}

Stack* ponyint_stack_pop(Stack* stack, void** data)
{
  pony_assert(stack != NULL);
  pony_assert(data != NULL);

  stack->index--;
  *data = stack->data[stack->index];

  if(stack->index == 0)
  {
    Stack* prev = stack->prev;
    POOL_FREE(Stack, stack);
    return prev;
  }

  return stack;
}

Stack* ponyint_stack_push(Stack* stack, void* data)
{
  if((stack != NULL) && (stack->index < STACK_COUNT))
  {
    stack->data[stack->index] = data;
    stack->index++;
  } else {
    stack = stack_new(stack, data);
  }

  return stack;
}

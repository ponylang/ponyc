#include <stdint.h>

static int32_t initialized = 0;

__attribute__((constructor))
static void shim_init(void)
{
  initialized = 42;
}

int32_t shim_constructor_ran(void)
{
  return initialized;
}

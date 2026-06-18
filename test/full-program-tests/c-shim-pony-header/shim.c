#include <stdint.h>
#include <pony.h>

/* pony.h is on the include path by default, resolved relative to the
 * running compiler like stdlib discovery, so shims can call runtime APIs
 * without any cincludedir: configuration. Called over FFI from an actor, the
 * current thread has a scheduler context. */
int32_t shim_has_ctx(void)
{
  return (pony_ctx() != NULL) ? 42 : 1;
}

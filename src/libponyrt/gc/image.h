#ifndef gc_image_h
#define gc_image_h

#include <platform.h>
#include <stdbool.h>
#include <stdint.h>

PONY_EXTERN_C_BEGIN

// The part of the image holding the runtime that is read-only once the
// program is loaded. Both are 0 until ponyint_image_init runs.
extern uintptr_t ponyint_image_ro_start;
extern uintptr_t ponyint_image_ro_end;

// Finds the read-only part of the image that holds the runtime. Call it once,
// before any scheduler thread starts.
void ponyint_image_init(void);

// Whether p is in memory that is read-only once the program is loaded. The
// compiler's constants, such as string literals, are there. Such memory can't
// hold a reference to anything allocated at run time, so the GC has nothing
// to do for an object there.
static inline bool ponyint_image_constant(const void* p)
{
  return ((uintptr_t)p - ponyint_image_ro_start) <
    (ponyint_image_ro_end - ponyint_image_ro_start);
}

PONY_EXTERN_C_END

#endif

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
// before any scheduler thread starts and before any actor is created: actor
// lookup (ponyint_actor_containing) uses the bounds to recognise descriptors,
// and pony_create and actor destruction use them to decide whether an actor
// has a pagemap entry, so the two must see the same bounds.
void ponyint_image_init(void);

// Whether p is in the runtime's image, in memory that is read-only once the
// program is loaded. The compiler's constants, such as string literals, are
// there. That memory can't hold a pointer written at run time, so the GC
// doesn't trace an object there. An object there that points to writable
// memory holding Pony references isn't traced through either.
static inline bool ponyint_image_constant(const void* p)
{
  return ((uintptr_t)p - ponyint_image_ro_start) <
    (ponyint_image_ro_end - ponyint_image_ro_start);
}

PONY_EXTERN_C_END

#endif

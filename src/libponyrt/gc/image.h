#ifndef gc_image_h
#define gc_image_h

#include <platform.h>
#include <stdbool.h>
#include <stdint.h>

PONY_EXTERN_C_BEGIN

// The bounds of the part of the image holding the runtime that is read-only
// once the program is loaded. Both are 0 until ponyint_image_init runs.
extern uintptr_t ponyint_image_ro_start;
extern uintptr_t ponyint_image_ro_end;

// Finds the read-only part of the image that holds the runtime. Call it once,
// before any scheduler thread starts and before any actor is created. The
// bounds must not change while actors exist: whether an actor has a pagemap
// entry is decided from them when it is created and again when it is
// destroyed.
void ponyint_image_init(void);

#if defined(__ELF__) && !defined(PLATFORM_IS_MACOSX) && \
  !defined(PLATFORM_IS_WINDOWS)
struct dl_phdr_info;

// The read-only part of the loaded object described by info, if it contains
// self; page_size is the size of a page of memory. False if it doesn't contain
// self.
bool ponyint_image_elf_bounds(struct dl_phdr_info* info, uintptr_t self,
  uintptr_t page_size, uintptr_t* ro_start, uintptr_t* ro_end);
#endif

// Whether p is in the runtime's image, in memory that is read-only once the
// program is loaded. When the runtime is linked into the program, the
// compiler's constants, such as string literals, are there. That memory can't
// hold a pointer written at run time, so the GC doesn't trace an object there,
// nor through it to writable memory holding Pony references.
static inline bool ponyint_image_constant(const void* p)
{
  return ((uintptr_t)p - ponyint_image_ro_start) <
    (ponyint_image_ro_end - ponyint_image_ro_start);
}

// Whether all of a type descriptor is in that memory, so it can't change and
// every field of it can be read.
static inline bool ponyint_image_descriptor(const void* type, size_t size)
{
  return ponyint_image_constant(type) &&
    ponyint_image_constant((const char*)type + size - 1);
}

PONY_EXTERN_C_END

#endif

#ifndef mem_alloc_h
#define mem_alloc_h

/**
 * Allocates memory in the virtual address space.
 */
void* ponyint_virt_alloc(size_t bytes);

/**
 * Deallocates a chunk of memory that was previously allocated with
 * ponyint_virt_alloc.
 */
void ponyint_virt_free(void* p, size_t bytes);

/**
 * Advises the OS that the given memory range is no longer needed, allowing
 * it to reclaim the physical pages. The virtual address range remains valid
 * and accessible — pages are transparently faulted back in on next access.
 * Silently continues on failure since this is a memory optimization, not a
 * correctness requirement.
 */
void ponyint_virt_decommit(void* p, size_t bytes);

/**
 * Returns the system page size in bytes. The value is cached after the first
 * call.
 */
size_t ponyint_page_size();

#endif

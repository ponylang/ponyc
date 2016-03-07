#ifndef PLATFORM_ALLOC_H
#define PLATFORM_ALLOC_H

/**
 * Allocates memory in the virtual address space.
 */
void* ponyint_virt_alloc(size_t bytes);

/**
 * Deallocates a chunk of memory that was previously allocated with
 * ponyint_virt_alloc.
 */
void ponyint_virt_free(void* p, size_t bytes);

#endif

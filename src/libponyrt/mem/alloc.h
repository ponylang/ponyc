#ifndef PLATFORM_ALLOC_H
#define PLATFORM_ALLOC_H

/**
 * Allocates memory in the virtual address space.
 */
void* virtual_alloc(size_t bytes);

/**
 * Deallocates a chunk of memory that was previously allocated with
 * virtual_alloc.
 */
bool virtual_free(void* p, size_t bytes);

#endif

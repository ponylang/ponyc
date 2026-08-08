#ifndef mem_alloc_h
#define mem_alloc_h

#include <stddef.h>

#include <platform.h>

PONY_EXTERN_C_BEGIN

/**
 * Allocates memory in the virtual address space.
 */
void* ponyint_virt_alloc(size_t bytes);

/**
 * Reserves bytes of address space aligned to its own size. The size must be
 * a power of two, a multiple of the page size, and at least 64 KiB. The
 * reservation is uncommitted: a range must be given to ponyint_virt_commit
 * before it is touched.
 * Returns NULL on failure where ponyint_virt_alloc aborts: an allocator
 * holding freed memory in reserve can respond to exhaustion by giving some
 * back and retrying, which an abort inside the primitive would forbid.
 */
void* ponyint_virt_reserve_aligned(size_t size);

/**
 * Makes a range of a reservation from ponyint_virt_reserve_aligned usable.
 * Aborts when the range cannot be backed, rather than returning a failure
 * every caller would abort on anyway.
 */
void ponyint_virt_commit(void* p, size_t bytes);

/**
 * Deallocates a chunk of memory that was previously allocated with
 * ponyint_virt_alloc or ponyint_virt_reserve_aligned.
 */
void ponyint_virt_free(void* p, size_t bytes);

/**
 * Advises the OS that the given memory range is no longer needed, allowing
 * it to reclaim the physical pages. The range stays usable: the caller may
 * write to it again without asking for it back, and what it reads until then
 * is undefined. Silently continues on failure since this is a memory
 * optimization, not a correctness requirement.
 */
void ponyint_virt_discard(void* p, size_t bytes);

/**
 * Gives a range of a reservation from ponyint_virt_reserve_aligned back to
 * the OS, keeping its address space reserved. The range must be given to
 * ponyint_virt_commit again before it is touched. Where ponyint_virt_discard
 * leaves a range the caller may write to whenever it likes, this one does
 * not: what a caller that skips the commit gets is undefined and differs by
 * platform, so the commit is owed on every path. Silently continues on
 * failure, which leaves the range committed and its memory unreturned.
 */
void ponyint_virt_decommit(void* p, size_t bytes);

/**
 * Returns the system page size in bytes. The value is cached after the first
 * call.
 */
size_t ponyint_page_size();

PONY_EXTERN_C_END

#endif

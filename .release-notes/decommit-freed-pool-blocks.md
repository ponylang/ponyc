## Return memory to the OS when freeing large pool allocations

The pool allocator now returns physical memory to the OS when freeing allocations larger than 1 MB. Previously, freed blocks were kept on the free list with their physical pages committed, meaning the RSS never decreased even after the memory was no longer needed. Now, the page-aligned interior of freed blocks is decommitted via `madvise(MADV_DONTNEED)` on POSIX or `VirtualAlloc(MEM_RESET)` on Windows. The virtual address space is preserved, so pages fault back in transparently on reuse.

This primarily benefits programs that make large temporary allocations (the compiler during compilation, programs with large arrays or strings). Programs whose memory usage is dominated by small allocations (< 1 MB) will see little change — per-size-class caching is a separate mechanism.

To preserve the old behavior (never decommit), build with `make configure use=pool_retain`.

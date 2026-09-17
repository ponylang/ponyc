## Fix out-of-memory crash on ILP32 Linux from address space fragmentation

On 32-bit Linux (ILP32), compiling large programs could crash with "out of memory" well before hitting the physical memory limit. The pool arena's aligned memory allocation mapped twice the requested size to find an aligned boundary, then unmapped the excess. Each 64MB region consumed 128MB of address space, fragmenting the 3GB user space so badly that allocation failed at roughly 1.5GB of actual use.

The allocator now uses `MAP_FIXED_NOREPLACE` (available since Linux 4.17, within ponyc's existing 5.3 kernel minimum) to request aligned addresses directly, avoiding the overallocation. It falls back to the old approach when the aligned probes fail.

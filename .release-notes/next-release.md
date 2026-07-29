## The runtime no longer reserves SIGUSR2

The runtime used SIGUSR2 to wake sleeping scheduler threads on Linux and the BSDs, so it reserved the signal for its own use: the signals package could not touch it, and `Sig.usr2()` was a compile error on those platforms. The scheduler no longer uses SIGUSR2, so the runtime no longer reserves it. `Sig.usr2()` now returns the signal number on Linux and the BSDs, and a program can subscribe to SIGUSR2 like any other signal. macOS never reserved it, so nothing changes there.

## Replace the runtime allocator

The runtime's pool allocator couldn't return freed memory to the operating system, reuse a large block on a thread that didn't free it, or re-carve memory from one size class for another. A program that passed large blocks between threads reserved fresh address space for every block it freed and grew without bound.

Every platform now uses a new allocator in which every piece of memory has an owning thread, following the design in [discussion #5735](https://github.com/ponylang/ponyc/discussions/5735). Memory comes from the operating system in large shared regions that threads carve into arenas; freed memory is reused across threads and size classes, and an emptied arena's physical memory goes back to the operating system while its address space is kept for reuse.

The previous allocator stays available behind a new build option:

```bash
cmake --preset release -DPONY_USES=pool_classic
```

Building with `address_sanitizer`, `valgrind`, or `pooltrack` now requires pairing with `pool_classic` (or `pool_memalign` for AddressSanitizer), since none of the three can track the new allocator's memory. `pool_retain` stops the classic pool returning memory to the operating system, so it requires `pool_classic` too. The build stops with an error saying so.

## Add the --ponymemoryprofile runtime option

The new allocator trades resident memory against throughput by how promptly it returns freed memory to the operating system, and `--ponymemoryprofile` picks where on that trade a program runs. It takes a number from 1 to 10: 1 returns memory quickly for the smallest footprint, 10 holds it for the most throughput, and the default sits in the balanced middle. Today the mapping is coarse -- 1 through 3 are the low-memory behavior, 4 through 7 balanced, and 8 through 10 throughput.

```bash
./my-program --ponymemoryprofile=8
```

A program can also set it in code through the `RuntimeOptions` struct, the same as the other runtime options. The option affects only the new allocator; a program built with `pool_classic` ignores it.

## Remove the scheduler_scaling_pthreads build option

The `scheduler_scaling_pthreads` build option selected one of two mechanisms the runtime used to pause and resume idle scheduler threads. The scheduler no longer uses either mechanism, so the option is gone: a build that passes `use=scheduler_scaling_pthreads` now fails. Systematic testing no longer pairs with it — build with `use=systematic_testing` alone.

## Fix use after free bug in actor heap finalisation that can lead to a segfault

The [0.45.2](https://github.com/ponylang/ponyc/releases/tag/0.45.2) release introduced an improvement to handling of objects with finalisers to make them more efficient to allocate on actor heaps. However, in the process it also introduced a potential for use after free situations that could lead to segfaults when running finalisers. With this change, we've reworked the implementation of the actor heap garbage collection to avoid the potential for use after free situations entirely.

## Fix actor heap chunk size tracking bug that could cause a segfault

The [0.55.1](https://github.com/ponylang/ponyc/releases/tag/0.55.1) release included some internal actor heap implementation optimizations. Unfortunately, there was a small bug that could potentially cause a segfault due to not properly clearing some bits before setting them for some heap chunks. This change corrects that oversight to ensure the relevant bits are properly cleared before being set to ensure they final result can never be incorrect.


## Fix use after free bug in actor heap finalisation that can lead to a segfault

The [0.45.2](https://github.com/ponylang/ponyc/releases/tag/0.45.2) release introduced an improvement to handling of objects with finalisers to make them more efficient to allocate on actor heaps. However, in the process it also introduced a potential for use after free situations that could lead to segfaults when running finalisers. With this change, we've reworked the implementation of the actor heap garbage collection to avoid the potential for use after free situations entirely.


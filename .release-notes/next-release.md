## Fixed an overflow in Time.nanos() values on Windows

Based on how were were calculating nanos based of the QueryPerformanceCounter and QueryPerformanceFrequency on Windows, it was quite likely that we would eventually overflow the U64 we used to represent nanos and "go backwards in time".

## Fix incorrect interaction between String/Array reserve and Pointer realloc

This was an interesting one. So, there's was an interesting interaction between String and Array's reserve and Pointer's realloc. Generally, it went like this:

`reserve` when it called `realloc` would give `realloc` a single value, the size of the newly reserved chunk of memory. This would result in `realloc` copying not just the in-use items from old memory to newly allocated memory, but in fact, the entire old memory chunk.

This could lead to very surprising results if `reserve` had been called by `undefined`. And in general, it could lead to suboptimal performance as we could end up copying more than was needed.

`realloc` has been updated to take to parameters, now, the amount of memory to alloc and the number of bytes to copy from old memory to new.


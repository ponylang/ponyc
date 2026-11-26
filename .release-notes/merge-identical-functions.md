## Reduce memory use, build time, and binary size of optimized builds

ponyc now keeps one copy of functions whose compiled code is identical, such as the same method of a generic class used with different type arguments. In optimized builds ponyc hands the linker less code, so linking uses less memory, builds finish sooner, and binaries are smaller. Building the standard library's test program on Linux x86-64, peak memory dropped from about 11.7 GB to 8.9 GB, build time from about 246 seconds to 153, and the binary from 129 MB to 77 MB.

In a debugger or profiler, one of these functions can appear under the name of another function with the same code. Debug builds (`-d`) are unchanged.

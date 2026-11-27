## Use less memory while linking

ponyc now releases the memory it used for code generation before the linker starts, so ponyc and the linker no longer hold that memory at the same time. Building the standard library's test program with optimizations, the peak memory of ponyc and the linker together dropped from about 8.9 GB to 7.3 GB on glibc-based Linux x86-64 and from about 7.3 GB to 6.7 GB on Windows x86-64. On macOS the peak didn't change. Builds on Linux distributions that use musl, such as Alpine, weren't measured.

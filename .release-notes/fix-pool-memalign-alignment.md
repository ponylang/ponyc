## Fix pool_memalign crash due to insufficient alignment for AVX instructions

`pool_memalign` used `malloc()` for allocations smaller than 1024 bytes. On x86-64 Linux, `malloc()` only guarantees 16-byte alignment, but the Pony runtime uses AVX instructions that require 32-byte alignment. This caused a SIGSEGV on startup for any program built with `-DUSE_POOL_MEMALIGN`. All allocations now use `posix_memalign()` with proper alignment.

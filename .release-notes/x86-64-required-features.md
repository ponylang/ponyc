## Fix crashes when disabling required x86-64 CPU features

On x86-64, passing `--features=-sse`, `-sse2`, `-64bit`, or `-64bit-mode` could crash the compiler or fail during linking. ponyc now keeps these required features enabled, even when `--features` requests disabling them. This follows the existing behavior for `cx16` on x86-64 and `sse2` on 32-bit x86.

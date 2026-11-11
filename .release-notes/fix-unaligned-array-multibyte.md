## Fix misaligned loads and stores in Array[U8] multi-byte accessors

`Array[U8]`'s multi-byte methods (`read_u16` through `read_u128`, `update_u16` through `update_u128`, and `push_u16` through `push_u128`) generated loads and stores assuming the wider type's alignment at an arbitrary byte offset. On i386 with SSE2, the `u128` operations crashed with a segfault at unaligned offsets; `buffered.Reader` and `buffered.Writer` u128 operations crashed the same way. On other targets the operations could silently produce wrong results, even when they didn't crash.

The twelve methods now work correctly at any byte offset.

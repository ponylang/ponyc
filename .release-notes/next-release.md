## Make actor heap allocations more efficient by recycling freed memory

Prior to this change, the actor heap garbage collection process would return freed memory back to the pony runtime at the end of a garbage collection run. The returning of memory to the pony runtime and allocating of new memory from the pony runtime are both expensive operations. This change makes it so that the actor garbage collection process keeps old freed memory chunks around with the expectation that the actor will very likely need memory again as it continues to run behaviors. This avoids the expensive return to and reallocation of memory from the pony runtime. It is possible that the overall application might end up using more memory as any freed memory chunks can only be reused by the actor that owns them and the runtime and other actors can no longer reuse that memory as they previously might have been able to.

## Correctly find custom-built `llc` during build process

Previously our CMake build was failing to find our custom-built `llc` binary, but builds worked by accident if there was a system `llc`.  The build system now finds our custom `llc` correctly.

## Fix minor buffer out of bounds access bug in compiler

Previously, the compiler error reporting had a minor buffer out of bounds access bug where it read one byte past the end of a buffer as part of an if condition before checking that the offset was smaller than the buffer length. This was fixed by switching the order of the if condition checks so that the check that the offset is smaller than the buffer length happens before reading the value from the buffer to prefer the out of bounds access issue.

## Fix bug in ASIO shutdown

There was a bug during our shutdown process that could cause a segmentation fault coming from our asynchrnous I/O subsystem. This has been fixed.


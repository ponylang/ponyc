## Fix SEGV when sending a union of tuples to an actor

Sending a message with a union of tuple types would crash with a segfault when one tuple variant contained a class or actor reference at a position where another variant contained a machine word type. For example, `((U32, String) | (U32, U32))` would crash when sending the `(U32, U32)` variant because the GC trace code would try to dereference the second `U32` value as a pointer.

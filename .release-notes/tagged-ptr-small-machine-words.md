## Don't box machine words smaller than 64 bits

On 64-bit platforms, Bool, U8, I8, U16, I16, U32, I32, and F32 are no longer heap-allocated when they appear in union types. On Windows (LLP64), ILong and ULong are also covered since `long` is 32-bit on that platform.

This eliminates a heap allocation every time one of these types appears in a union like `(U32 | None)` or `Any`, reducing GC pressure. Unboxing, match discrimination, and identity comparison no longer dereference a heap object for these types.

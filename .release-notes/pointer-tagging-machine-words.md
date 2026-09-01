## Don't heap-allocate small numeric types used as interfaces or union members

When a small numeric type (Bool, U8, I8, U16, I16, U32, I32, or F32) is used where a boxed representation is required — passed through an interface, stored in a union, or sent as `Any val` — the compiler previously allocated a heap object to hold the value. On 64-bit platforms, these values now use pointer tagging instead: the value is encoded directly in the pointer, so no allocation happens.

Pattern matching, identity comparison (`is`/`isnt`), interface dispatch, and cross-actor message sends all work transparently with tagged pointers. Types wider than 32 bits (I64, U64, F64, I128, U128, ILong, ULong, ISize, USize) still use heap allocation.

Programs that box small numeric values heavily — serialization, generic collections, message-passing pipelines — will see reduced GC pressure and fewer allocations. Programs that rarely box see no measurable change; the per-descriptor-fetch overhead is a branchless select (~9 ALU ops) that modern pipelines absorb.

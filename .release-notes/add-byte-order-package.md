## Add byte_order standard library package

A new `byte_order` package provides `BigEndian` and `LittleEndian` primitives for converting multi-byte values to and from their wire representation.

Read methods decode from any `ReadSeq[U8]` source (with an optimized path for `Array[U8]`). Write methods append encoded bytes to an `Array[U8]`. Both primitives cover U16–U128, their signed counterparts, and F32/F64.

```pony
use "byte_order"

// Encode a U32 in network (big-endian) order
let buf = Array[U8]
BigEndian.push_u32(buf, 0xDEADBEEF)
// buf is now [0xDE; 0xAD; 0xBE; 0xEF]

// Decode it back
let value = BigEndian.u32[Array[U8] box](buf)?  // 0xDEADBEEF
```

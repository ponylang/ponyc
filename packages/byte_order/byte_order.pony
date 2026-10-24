"""
# Byte Order package

Stateless byte-order conversion between native integers and their
big-endian or little-endian wire representations.

Two primitives — `BigEndian` and `LittleEndian` — each provide the same set
of methods:

* **Read** (`u16`, `u32`, `u64`, `u128`, and signed/float variants) —
  decode a value from any `ReadSeq[U8]` at a given offset.
* **Write** (`push_u16`, `push_u32`, `push_u64`, `push_u128`, and
  signed/float variants) — encode a value by appending bytes to an
  `Array[U8]`.

Pick the primitive that matches the wire format you are reading or writing.
Network protocols almost always use big-endian; file formats vary — check
the spec.

When the sequence is an `Array[U8]`, reads are optimized to a single
native-width load plus a conditional byte swap, which the compiler reduces
to a single instruction on architectures that support it.

When the sequence is not an `Array[U8]`, reads fall back to a portable
shift-and-OR implementation that works for any `ReadSeq[U8]`.

Read methods are partial: they raise an error when the sequence does not
contain enough bytes at the given offset.

## Example

```pony
use "byte_order"

actor Main
  new create(env: Env) =>
    let buf = Array[U8]
    BigEndian.push_u32(buf, 0xDEADBEEF)
    // buf is [0xDE; 0xAD; 0xBE; 0xEF]

    try
      let v = BigEndian.u32[Array[U8] box](buf)?
      env.out.print(v.string())  // 3735928559
    end
```
"""

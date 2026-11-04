"""
# Binary Codec Package

A combinator library for bidirectional binary parsing and building.
Define a `Codec[A]` once and use it to both decode bytes into `A`
and encode `A` back to bytes.

Codecs are `val` — immutable and shareable across actors. Small
codecs compose into larger ones:

- `Seq2` through `Seq6` sequence fields into a tuple.
- `Mapped` maps a wire type to a domain type and back.
- `BitsU8`, `BitsU16`, `BitsU32`, `BitsU64` read sub-byte fields.
- `FlatZip` decodes a value, then uses it to pick the next codec.
- `Count` reads a count and decodes that many elements.
- `Many` repeats a codec until the input runs out.
- `Discriminated` dispatches on a tag to decode a tagged union.
- `Literal`, `ConstThen`, `ThenConst` match or emit fixed bytes.
- `VariableSize` reads a length, then decodes a body of that size.
- `Conditional` includes a field only when a flag is true.
- `SkipBits` skips padding bits.
- `FrameDecoder` wraps any codec for TCP stream framing.

Every combinator is itself a `Codec`, so compositions nest: the
codec inside a `Count` can be a `Discriminated` that contains an
`Mapped` wrapping a `Seq4`.

`decode` takes an `Array[U8] val` and a bit offset. It returns
`(A, USize)` — the value and the number of bits consumed — or a
`DecodeError`. The input array is `val`, so byte-slice codecs
return zero-copy views via `Array.trim`.

`encode` converts a value to bytes, returning `Array[U8] val` or
an `EncodeError`.

Errors are data: `InsufficientData` means the input is incomplete
(wait for more bytes); `MalformedData` means the data is
structurally wrong (a literal mismatch, an unknown tag, a
constraint violation). `FrameDecoder` treats `InsufficientData` as
"wait for more" and `MalformedData` as a protocol error.

## Example: fixed-layout struct

`Seq4` sequences four fields into a tuple. `Mapped` maps the tuple to
a domain class and back. The result is a single codec that both
decodes and encodes UDP headers.

```pony
use "binary_codec"

class val UdpHeader
  let src_port: U16
  let dst_port: U16
  let length: U16
  let checksum: U16

  new val create(s: U16, d: U16, l: U16, c: U16) =>
    src_port = s; dst_port = d; length = l; checksum = c

actor Main
  new create(env: Env) =>
    let codec: Codec[UdpHeader] =
      Mapped[(U16, U16, U16, U16), UdpHeader](
        Seq4[U16, U16, U16, U16](U16BE, U16BE, U16BE, U16BE),
        {(t) => UdpHeader(t._1, t._2, t._3, t._4)},
        {(h) => (h.src_port, h.dst_port, h.length, h.checksum)})

    // Decode
    let data: Array[U8] val =
      [0x00; 0x50; 0x00; 0x51; 0x00; 0x08; 0x12; 0x34]

    match codec.decode(data, 0)
    | (let h: UdpHeader, _) =>
      env.out.print("src=" + h.src_port.string())  // 80
      env.out.print("dst=" + h.dst_port.string())  // 81
    | InsufficientData =>
      env.out.print("need more data")
    | let e: MalformedData =>
      env.out.print("bad data: " + e.string())
    end

    // Encode
    match codec.encode(UdpHeader(80, 81, 8, 0x1234))
    | let bytes: Array[U8] val =>
      // bytes is the same 8 bytes as data above
      None
    | let e: EncodeError =>
      env.out.print("encode failed: " + e.string())
    end
```

## Example: sub-byte bit fields

`BitsU8(n)` reads `n` bits (1–8) as a `U8`. Fields at non-byte
widths compose with `Seq` the same way byte-aligned fields do.
The first two bytes of an IPv4 header contain four fields that
do not fall on byte boundaries:

```pony
// version(4 bits) + IHL(4) + DSCP(6) + ECN(2) = 16 bits
let ipv4_prefix =
  Seq4[U8, U8, U8, U8](
    BitsU8(4), BitsU8(4), BitsU8(6), BitsU8(2))

// 0x45 = version 4, IHL 5; 0x00 = DSCP 0, ECN 0
let data: Array[U8] val = [0x45; 0x00]
match ipv4_prefix.decode(data, 0)
| ((let ver: U8, let ihl: U8, let dscp: U8, let ecn: U8), _) =>
  // ver == 4, ihl == 5, dscp == 0, ecn == 0
end
```

For wider sub-byte fields, use `BitsU16(n)` (1–16 bits),
`BitsU32(n)` (1–32), or `BitsU64(n)` (1–64).

## Example: dependent parsing (FlatZip)

`FlatZip` decodes a value and passes it to a function that returns
the codec for the next field. This handles formats where a length
or type field determines what follows.

```pony
// A U8 byte count followed by that many bytes
let length_prefixed =
  FlatZip[U8, Array[U8] val](
    U8Codec,
    {(len: U8): Codec[Array[U8] val] =>
      Bytes(len.usize())})

// 0x03 means 3 bytes follow
let data: Array[U8] val = [0x03; 0x41; 0x42; 0x43]
match length_prefixed.decode(data, 0)
| ((let len: U8, let body: Array[U8] val), _) =>
  // len == 3, body == [0x41; 0x42; 0x43]
end
```

## Example: count-prefixed array (Count)

`Count` reads a `U16` count field, then decodes that many
instances of a body codec into an `Array`.

```pony
let records = Count[U32](U16BE, U32BE)

// Count = 2, then two U32BE values (10 and 20)
let data: Array[U8] val =
  [0x00; 0x02; 0x00; 0x00; 0x00; 0x0A; 0x00; 0x00; 0x00; 0x14]
match records.decode(data, 0)
| (let arr: Array[U32] val, _) =>
  // arr.size() == 2, arr(0)? == 10, arr(1)? == 20
end
```

## Example: fixed byte sequence (ConstThen)

`Literal` matches a fixed byte sequence on decode and emits it on
encode. `ConstThen` pairs a `Literal` before a value codec,
keeping only the value. `ThenConst` does the reverse — value
first, then literal.

```pony
// Match MPEG sync byte 0x47, then read a payload byte
let sync = ConstThen[U8](Literal([as U8: 0x47]), U8Codec)

match sync.decode([as U8: 0x47; 0xFF], 0)
| (let v: U8, _) => // v == 0xFF
end

// Wrong sync byte produces MalformedData
match sync.decode([as U8: 0x00; 0xFF], 0)
| let e: MalformedData => // literal mismatch
end
```

## Example: tagged union (Discriminated)

`Discriminated` reads a tag and dispatches to a variant codec.
Register each variant with `DiscriminatedBuilder`: a tag value,
a codec for the variant body, and a narrowing function that casts
from the union type to the variant.

```pony
use "binary_codec"

type Command is (Ping | Data)

class val Ping
  let seq: U16
  new val create(s: U16) => seq = s

class val Data
  let payload: Array[U8] val
  new val create(p: Array[U8] val) => payload = p

// Tag 0x01 → Ping (U16BE body)
// Tag 0x02 → Data (remaining bytes)
let builder = DiscriminatedBuilder[U8, Command](U8Codec)
builder
  .case_of[Ping](0x01,
    Mapped[U16, Ping](U16BE,
      {(s) => Ping(s)}, {(p) => p.seq}),
    {(c: Command): Ping ? => c as Ping})
  .case_of[Data](0x02,
    Mapped[Array[U8] val, Data](BytesRest,
      {(b) => Data(b)}, {(d) => d.payload}),
    {(c: Command): Data ? => c as Data})
let codec = builder.build()

match codec.decode([as U8: 0x01; 0x00; 0x42], 0)
| (let p: Ping, _) => // p.seq == 66
| (let d: Data, _) => // not this branch
end

// Encode round-trips through the same codec
match codec.encode(Ping(66))
| let bytes: Array[U8] val =>
  // bytes == [0x01; 0x00; 0x42]
end
```

## Example: TCP stream framing (FrameDecoder)

`FrameDecoder` wraps a codec for TCP streams. Feed it `iso` byte
chunks as they arrive; it buffers incomplete data across calls and
delivers complete decoded values through `FrameNotify`.

```pony
use "binary_codec"

class ref MyNotify is FrameNotify[U16]
  fun ref frame_received(value: U16) =>
    // called once per complete U16 from the stream
    None

  fun ref frame_error(err: MalformedData) =>
    None

// Wrap U16BE as a stream codec
let framer = FrameDecoder[U16](U16BE)
let notify = MyNotify

// First chunk: one byte — not enough for a U16
let c1: Array[U8] iso = recover [as U8: 0x00] end
framer.received(consume c1, notify)
// Nothing fires. The byte is buffered.

// Second chunk: two more bytes
let c2: Array[U8] iso = recover [as U8: 0x42; 0x00] end
framer.received(consume c2, notify)
// frame_received(0x0042) fires (from buffered 0x00 + new 0x42).
// Remaining 0x00 is buffered for the next call.
```
"""

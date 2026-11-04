## Add binary_codec package

A new standard library package for bidirectional binary parsing. Define a `Codec[A]` once — the same definition decodes bytes into a value and encodes the value back to bytes.

Sub-byte fields (4-bit nibbles, 1-bit flags, 13-bit offsets) work the same as byte-aligned fields. `decode` takes `Array[U8] val` so byte-slice codecs return zero-copy views via `Array.trim`.

Codecs are `val` and compose via combinators: `Seq2`–`Seq6` sequence fields into tuples, `Mapped` maps between wire tuples and domain types, `FlatZip` threads a decoded value into a dependent codec, `Many` and `Count` handle repetition, and `Discriminated` dispatches on a tag value.

Decode errors are data: `InsufficientData` means the input is incomplete, `MalformedData` means the data is structurally wrong. `FrameDecoder` wraps any codec for TCP stream framing and uses this distinction to separate "not enough data yet" from "corrupt message."

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

    let data: Array[U8] val =
      [0x00; 0x50; 0x00; 0x51; 0x00; 0x08; 0x12; 0x34]

    match codec.decode(data, 0)
    | (let h: UdpHeader, let bits: USize) =>
      env.out.print("src=" + h.src_port.string())
      env.out.print("dst=" + h.dst_port.string())
    | InsufficientData =>
      env.out.print("need more data")
    | let e: MalformedData =>
      env.out.print("bad data: " + e.string())
    end
```

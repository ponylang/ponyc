"""
# Struct package

This package performs conversions between Pony values and C structs represented
as Pony ByteSeq objects. This can be used in handling binary data stored in
files or from network connections, among other sources. It uses Format Strings
as compact descriptions of the layout of the C structs and the intended
conversion to/from Pony values.

It is inspired by the Python Struct module:
https://docs.python.org/3.7/library/struct.html

## Usage

### Struct.pack
Pack an `Array[(Bool | Number | String | Array[U8] val)]` according to a format
string:

```pony
let byte_seqs = Struct.pack(">Iqd5ss5p",
  [ 1024
    1544924117813084
    3.141592653589793
    "hello"
    "!"
    recover [as U8: 1;2;3;4;5] end
    ] )?
```

### Struct.unpack
Unpack a bytestream (a buffered Reader) into an
`Array[(Bool | Number | String | Array[U8] val)]` according to a format string:

```pony
let unpacked = Struct.unpack(">Iq5s", consume reader)?
let length: U32 = unpacked(0)?
let nanosecond_offset = unpacked(1)? as I64
let message: String = unpacked(2)?
```

## Byte Order
Byte order can be optionally specified with the first character in the format
string.

| Byte order | character |
|     -      |     -     |
| Big Endian | >         |
| Little Endian | <      |


The format conversion specification is described in the table below.

| Format | C Type             | Pony type | Standard size |
|   -    |    -               |      -    |        -      |
| x	     | pad byte           |	no value  | 1             |
| c      | char               |	String    |	1	            |
| b      | signed char        | I8        | 1             |
| B	     | unsigned char      | U8        | 1             |
| ?      | _Bool              | Bool      | 1             |
| h      | short              | I16       | 2             |
| H      | unsigned short     | U16       | 2             |
| i      | int                | I32       | 4             |
| I      | unsigned int       | U32       | 4	            |
| l      | long               | I32       | 4             |
| L      | unsigned long      | U32       | 4             |
| q      | long long          | I64       | 8             |
| Q      | unsigned long long | U64       | 8             |
| u      | unsigned bigint    | U128      | 16            |
| f      | float              | f32       | 4             |
| d      | double             | f64       | 8             |
| s      | char[]             | String    |               |
| p      | char[]             | Array[U8] |               |


Each type may be preceded with an integer representing its length (in the case
of String and Array[U8]) or number repetitions (all other types).

All types passed to `pack` must have a reference capability of val.
"""

use "buffered"
use "collections"

type _Packable is (Bool | Number | String | Array[U8] val)

primitive _BigEndian
  fun string(): String => "BigEndian"
primitive _LittleEndian
  fun string(): String => "LittlEndian"
type Endian is (_BigEndian | _LittleEndian)

primitive Struct
  """
  Convert between native Pony types and bytestreams.
  """
  fun pack(fmt: String, args: Array[_Packable val], wb: Writer = Writer):
    Array[ByteSeq] ref^ ?
  =>
    """
    Convert an `Array[(Bool | Number | String | Array[U8] val)]` to an
    `Array[ByteSeq]` using a format specification string.
    """
    (let nd, let tspec, let pad_bytes) = _ParseFormat(fmt)?
    let big: Bool =
      match nd
      | _BigEndian => true
      else false
      end

    if (tspec.size() - pad_bytes) != args.size() then
      error
    end

    // We need an offset count for padding bytes to use when accessing
    // the args array with the tspec index
    var offset: USize = 0
    for idx in Range(0, tspec.size()) do
      let ts = tspec(idx)?
      let arg =
        try
          args(idx - offset)?
        else
          None
        end

      match (ts._1, arg)
      | ("x", _) =>
        wb.u8(0)
        offset = offset + 1
      | ("b", let a: I8) => wb.u8(a.u8())
      | ("B", let a: U8) => wb.u8(a)
      | ("?", let a: Bool) => if a then wb.u8(1) else wb.u8(0) end
      | ("h", let a: I16) => if big then wb.i16_be(a) else wb.i16_le(a) end
      | ("H", let a: U16) => if big then wb.u16_be(a) else wb.u16_le(a) end
      | ("i", let a: I32) => if big then wb.i32_be(a) else wb.i32_le(a) end
      | ("I", let a: U32) => if big then wb.u32_be(a) else wb.u32_le(a) end
      | ("l", let a: I32) => if big then wb.i32_be(a) else wb.i32_le(a) end
      | ("L", let a: U32) => if big then wb.u32_be(a) else wb.u32_le(a) end
      | ("q", let a: I64) => if big then wb.i64_be(a) else wb.i64_le(a) end
      | ("Q", let a: U64) => if big then wb.u64_be(a) else wb.u64_le(a) end
      | ("u", let a: I128) => if big then wb.i128_be(a) else wb.i128_le(a) end
      | ("U", let a: U128) => if big then wb.u128_be(a) else wb.u128_le(a) end
      | ("f", let a: F32) => if big then wb.f32_be(a) else wb.f32_le(a) end
      | ("d", let a: F64) => if big then wb.f64_be(a) else wb.f64_le(a) end
      | ("c", let a: (String | U8)) =>
        match a
        | let a': String => wb.write(a')
        | let a': U8 => wb.u8(a')
        end
      | ("s", let a: String) =>
        if a.size() != ts._2 then
          error
        end
        wb.write(a)
      | ("p", let a: Array[U8] val) =>
        if a.size() != ts._2 then
          error
        end
        wb.write(a)
      else
          error
      end
    end
    wb.done()

  fun unpack(fmt: String, rb: Reader): Array[_Packable val] ref^ ? =>
    """
    Convert a buffered `Reader` to an
    `Array[(Bool | Number | String | Array[U8] val)]` using a format
    specification string.
    """
    (let nd, let tspec, _) = _ParseFormat(fmt)?
    let big: Bool =
      match nd
      | _BigEndian => true
      else false
      end

    let a: Array[_Packable] ref = recover Array[_Packable] end
    for ts in tspec.values() do
      match ts._1
      | "x" => for i in Range(0, ts._2) do rb.skip(1)? end
      | "b" => for i in Range(0, ts._2) do a.push(rb.i8()?) end
      | "B" => for i in Range(0, ts._2) do a.push(rb.u8()?) end
      | "?" => for i in Range(0, ts._2) do
          a.push(if rb.u8()? == 1 then true else false end)
        end
      | "h" => for i in Range(0, ts._2) do
          a.push(if big then rb.i16_be()? else rb.i16_le()? end)
        end
      | "H" => for i in Range(0, ts._2) do
          a.push(if big then rb.u16_be()? else rb.u16_le()? end)
        end
      | "i" => for i in Range(0, ts._2) do
          a.push(if big then rb.i32_be()? else rb.i32_le()? end)
        end
      | "I" => for i in Range(0, ts._2) do
          a.push(if big then rb.u32_be()? else rb.u32_le()? end)
        end
      | "l" => for i in Range(0, ts._2) do
          a.push(if big then rb.i32_be()? else rb.i32_le()? end)
        end
      | "L" => for i in Range(0, ts._2) do
          a.push(if big then rb.u32_be()? else rb.u32_le()? end)
        end
      | "q" => for i in Range(0, ts._2) do
          a.push(if big then rb.i64_be()? else rb.i64_le()? end)
        end
      | "Q" => for i in Range(0, ts._2) do
          a.push(if big then rb.u64_be()? else rb.u64_le()? end)
        end
      | "u" => for i in Range(0, ts._2) do
          a.push(if big then rb.i128_be()? else rb.i128_le()? end)
        end
      | "U" => for i in Range(0, ts._2) do
          a.push(if big then rb.u128_be()? else rb.u128_le()? end)
        end
      | "f" => for i in Range(0, ts._2) do
          a.push(if big then rb.f32_be()? else rb.f32_le()? end)
        end
      | "d" => for i in Range(0, ts._2) do
          a.push(if big then rb.f64_be()? else rb.f64_le()? end)
        end
      | "c" => for i in Range(0, ts._2) do
          a.push(String.from_array(rb.block(1)?))
        end
      | "s" => a.push(String.from_array(rb.block(ts._2)?))
      | "p" => a.push(rb.block(ts._2)?)
      else
          error
      end
    end
    consume a

primitive _ParseFormat
  fun apply(fmt: String): (Endian, Array[(String, USize)], USize) ? =>
    var padding_bytes: USize = 0
    var start_from: USize = 0
    let nd: Endian =
      if fmt.at("<", 0) then
        start_from = 1
        _LittleEndian
      elseif fmt.at(">", 0) then
        start_from = 1
        _BigEndian
      else
        _BigEndian
      end
    let a: Array[(String, USize)] ref = recover Array[(String, USize)] end
    var s: String = ""
    for i in Range(start_from, fmt.size()) do
      let c = String.from_array([fmt(i)?])
      if "xcbB?hHiIlLqQuUfdsp".contains(c) then
        if c == "x" then
          if s == "" then
            padding_bytes = padding_bytes + 1
            a.push((c, 1))
          else
            padding_bytes = padding_bytes + s.usize()?
            a.push((c, s.usize()?))
            s = ""
          end
        else
          if s == "" then
            a.push((c, 1))
          elseif "sp".contains(c) then
            a.push((c, s.usize()?))
            s = ""
          else
            for x in Range(0, s.usize()?) do
              a.push((c, 1))
            end
            s = ""
          end
        end
      else
        s = s + c
      end
    end
    (nd, consume a, padding_bytes)

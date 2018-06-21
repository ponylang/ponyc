"""
# Format package

The Format package provides support for formatting strings. It can be
used to set things like width, padding and alignment, as well as
controlling the way numbers are displayed (decimal, octal,
hexadecimal).

# Example program

```pony
use "format"

actor Main
  fun disp(desc: String, v: I32, fmt: FormatInt = FormatDefault): String =>
    Format(desc where width = 10)
      + ":"
      + Format.int[I32](v where width = 10, align = AlignRight, fmt = fmt)

  new create(env: Env) =>
    try
      (let x, let y) = (env.args(1)?.i32()?, env.args(2)?.i32()?)
      env.out.print(disp("x", x))
      env.out.print(disp("y", y))
      env.out.print(disp("hex(x)", x, FormatHex))
      env.out.print(disp("hex(y)", y, FormatHex))
      env.out.print(disp("x * y", x * y))
    else
      let exe = try env.args(0)? else "fmt_example" end
      env.err.print("Usage: " + exe + " NUMBER1 NUMBER2")
    end
```
"""

use "collections"

primitive Format
  """
  Provides functions for generating formatted strings.

  * fmt. Format to use.
  * prefix. Prefix to use.
  * prec. Precision to use. The exact meaning of this depends on the type,
  but is generally the number of characters used for all, or part, of the
  string. A value of -1 indicates that the default for the type should be
  used.
  * width. The minimum number of characters that will be in the produced
  string. If necessary the string will be padded with the fill character to
  make it long enough.
  *align. Specify whether fill characters should be added at the beginning or
  end of the generated string, or both.
  *fill: The character to pad a string with if is is shorter than width.
  """
  fun apply(
    str: String,
    fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault,
    prec: USize = -1,
    width: USize = 0,
    align: Align = AlignLeft,
    fill: U32 = ' ')
    : String iso^
  =>
    let copy_len = str.size().min(prec.usize())
    let len = copy_len.max(width.usize())
    recover
      let s = String(len)

      match align
      | AlignLeft =>
        s.append(str)
        for i in Range(s.size(), s.space()) do
          s.push_utf32(fill)
        end
      | AlignRight =>
        for i in Range(0, len - copy_len) do
          s.push_utf32(fill)
        end
        s.append(str)
      | AlignCenter =>
        let half = (len - copy_len) / 2
        for i in Range(0, half) do
          s.push_utf32(fill)
        end
        s.append(str)
        for i in Range(s.size(), s.space()) do
          s.push_utf32(fill)
        end
      end

      s .> recalc()
    end

  fun int[A: (Int & Integer[A])](
    x: A,
    fmt: FormatInt = FormatDefault,
    prefix: PrefixNumber = PrefixDefault,
    prec: USize = -1,
    width: USize = 0,
    align: Align = AlignRight,
    fill: U32 = ' ')
    : String iso^
  =>
    let zero = x.from[USize](0)
    (let abs, let neg) = if x < zero then (-x, true) else (x, false) end

    iftype A <: U128 then
      _FormatInt.u128(x.u128(), false, fmt, prefix, prec, width, align, fill)
    elseif A <: I128 then
      _FormatInt.u128(abs.u128(), neg, fmt, prefix, prec, width, align, fill)
    elseif A <: (U64 | I64) then
      _FormatInt.u64(abs.u64(), neg, fmt, prefix, prec, width, align, fill)
    elseif A <: (U32 | I32) then
      _FormatInt.u32(abs.u32(), neg, fmt, prefix, prec, width, align, fill)
    elseif A <: (U16 | I16) then
      _FormatInt.u16(abs.u16(), neg, fmt, prefix, prec, width, align, fill)
    elseif A <: (U8 | I8) then
      _FormatInt.u8(abs.u8(), neg, fmt, prefix, prec, width, align, fill)
    elseif A <: (USize | ISize) then
      ifdef ilp32 then
        _FormatInt.u32(abs.u32(), neg, fmt, prefix, prec, width, align, fill)
      else
        _FormatInt.u64(abs.u64(), neg, fmt, prefix, prec, width, align, fill)
      end
    elseif A <: (ULong | ILong) then
      ifdef ilp32 or llp64 then
        _FormatInt.u32(abs.u32(), neg, fmt, prefix, prec, width, align, fill)
      else
        _FormatInt.u64(abs.u64(), neg, fmt, prefix, prec, width, align, fill)
      end
    else
      _FormatInt.u128(x.u128(), false, fmt, prefix, prec, width, align, fill)
    end

  fun float[A: (Float & FloatingPoint[A])](
    x: A,
    fmt: FormatFloat = FormatDefault,
    prefix: PrefixNumber = PrefixDefault,
    prec: USize = 6,
    width: USize = 0,
    align: Align = AlignRight,
    fill: U32 = ' ')
    : String iso^
  =>
    _FormatFloat.f64(x.f64(), fmt, prefix, prec, width, align, fill)

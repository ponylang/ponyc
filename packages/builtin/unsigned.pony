use @"llvm.bitreverse.i8"[U8](src: U8)
use @"llvm.bitreverse.i16"[U16](src: U16)
use @"llvm.bitreverse.i32"[U32](src: U32)
use @"llvm.bitreverse.i64"[U64](src: U64)
use @"llvm.bitreverse.i128"[U128](src: U128)
use @"llvm.bswap.i8"[U16](src: U8)
use @"llvm.bswap.i16"[U16](src: U16)
use @"llvm.bswap.i32"[U32](src: U32)
use @"llvm.bswap.i64"[U64](src: U64)
use @"llvm.bswap.i128"[U128](src: U128)
use @"llvm.ctpop.i8"[U8](src: U8)
use @"llvm.ctpop.i16"[U16](src: U16)
use @"llvm.ctpop.i32"[U32](src: U32)
use @"llvm.ctpop.i64"[U64](src: U64)
use @"llvm.ctpop.i128"[U128](src: U128)
use @"llvm.ctlz.i8"[U8](src: U8, is_zero_undef: Bool)
use @"llvm.ctlz.i16"[U16](src: U16, is_zero_undef: Bool)
use @"llvm.ctlz.i32"[U32](src: U32, is_zero_undef: Bool)
use @"llvm.ctlz.i64"[U64](src: U64, is_zero_undef: Bool)
use @"llvm.ctlz.i128"[U128](src: U128, is_zero_undef: Bool)
use @"llvm.cttz.i8"[U8](src: U8, zero_undef: Bool)
use @"llvm.cttz.i16"[U16](src: U16, zero_undef: Bool)
use @"llvm.cttz.i32"[U32](src: U32, zero_undef: Bool)
use @"llvm.cttz.i64"[U64](src: U64, zero_undef: Bool)
use @"llvm.cttz.i128"[U128](src: U128, zero_undef: Bool)
use @"llvm.uadd.with.overflow.i8"[(U8, Bool)](a: U8, b: U8)
use @"llvm.uadd.with.overflow.i16"[(U16, Bool)](a: U16, b: U16)
use @"llvm.uadd.with.overflow.i32"[(U32, Bool)](a: U32, b: U32)
use @"llvm.uadd.with.overflow.i64"[(U64, Bool)](a: U64, b: U64)
use @"llvm.uadd.with.overflow.i128"[(U128, Bool)](a: U128, b: U128)
use @"llvm.usub.with.overflow.i8"[(U8, Bool)](a: U8, b: U8)
use @"llvm.usub.with.overflow.i16"[(U16, Bool)](a: U16, b: U16)
use @"llvm.usub.with.overflow.i32"[(U32, Bool)](a: U32, b: U32)
use @"llvm.usub.with.overflow.i64"[(U64, Bool)](a: U64, b: U64)
use @"llvm.usub.with.overflow.i128"[(U128, Bool)](a: U128, b: U128)
use @"llvm.umul.with.overflow.i8"[(U8, Bool)](a: U8, b: U8)
use @"llvm.umul.with.overflow.i16"[(U16, Bool)](a: U16, b: U16)
use @"llvm.umul.with.overflow.i32"[(U32, Bool)](a: U32, b: U32)
use @"llvm.umul.with.overflow.i64"[(U64, Bool)](a: U64, b: U64)
use @"llvm.umul.with.overflow.i128"[(U128, Bool)](a: U128, b: U128)

primitive U8 is UnsignedInteger[U8]
  new create(value: U8) => value
  new from[B: (Number & Real[B] val)](a: B) => a.u8()

  new min_value() => 0
  new max_value() => 0xFF

  fun next_pow2(): U8 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U8 => this
  fun bit_reverse(): U8 => @"llvm.bitreverse.i8"(this)
  fun bswap(): U8 => this
  fun popcount(): U8 => @"llvm.ctpop.i8"(this)
  fun clz(): U8 => @"llvm.ctlz.i8"(this, false)
  fun ctz(): U8 => @"llvm.cttz.i8"(this, false)

  fun clz_unsafe(): U8 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i8"(this, true)

  fun ctz_unsafe(): U8 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i8"(this, true)

  fun bitwidth(): U8 => 8

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: U8): U8 => if this < y then this else y end
  fun max(y: U8): U8 => if this > y then this else y end

  fun addc(y: U8): (U8, Bool) =>
    @"llvm.uadd.with.overflow.i8"(this, y)

  fun subc(y: U8): (U8, Bool) =>
    @"llvm.usub.with.overflow.i8"(this, y)

  fun mulc(y: U8): (U8, Bool) =>
    @"llvm.umul.with.overflow.i8"(this, y)

  fun divc(y: U8): (U8, Bool) =>
    _UnsignedCheckedArithmetic.div_checked[U8](this, y)

  fun remc(y: U8): (U8, Bool) =>
    _UnsignedCheckedArithmetic.rem_checked[U8](this, y)

  fun add_partial(y: U8): U8 ? =>
    _UnsignedPartialArithmetic.add_partial[U8](this, y)?

  fun sub_partial(y: U8): U8 ? =>
    _UnsignedPartialArithmetic.sub_partial[U8](this, y)?

  fun mul_partial(y: U8): U8 ? =>
    _UnsignedPartialArithmetic.mul_partial[U8](this, y)?

  fun div_partial(y: U8): U8 ? =>
    _UnsignedPartialArithmetic.div_partial[U8](this, y)?

  fun rem_partial(y: U8): U8 ? =>
    _UnsignedPartialArithmetic.rem_partial[U8](this, y)?

  fun divrem_partial(y: U8): (U8, U8) ? =>
    _UnsignedPartialArithmetic.divrem_partial[U8](this, y)?

primitive U16 is UnsignedInteger[U16]
  new create(value: U16) => value
  new from[A: (Number & Real[A] val)](a: A) => a.u16()

  new min_value() => 0
  new max_value() => 0xFFFF

  fun next_pow2(): U16 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U16 => this
  fun bit_reverse(): U16 => @"llvm.bitreverse.i16"(this)
  fun bswap(): U16 => @"llvm.bswap.i16"(this)
  fun popcount(): U16 => @"llvm.ctpop.i16"(this)
  fun clz(): U16 => @"llvm.ctlz.i16"(this, false)
  fun ctz(): U16 => @"llvm.cttz.i16"(this, false)

  fun clz_unsafe(): U16 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i16"(this, true)

  fun ctz_unsafe(): U16 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i16"(this, true)

  fun bitwidth(): U16 => 16

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: U16): U16 => if this < y then this else y end
  fun max(y: U16): U16 => if this > y then this else y end

  fun addc(y: U16): (U16, Bool) =>
    @"llvm.uadd.with.overflow.i16"(this, y)

  fun subc(y: U16): (U16, Bool) =>
    @"llvm.usub.with.overflow.i16"(this, y)

  fun mulc(y: U16): (U16, Bool) =>
    @"llvm.umul.with.overflow.i16"(this, y)

  fun divc(y: U16): (U16, Bool) =>
    _UnsignedCheckedArithmetic.div_checked[U16](this, y)

  fun remc(y: U16): (U16, Bool) =>
    _UnsignedCheckedArithmetic.rem_checked[U16](this, y)

  fun add_partial(y: U16): U16 ? =>
    _UnsignedPartialArithmetic.add_partial[U16](this, y)?

  fun sub_partial(y: U16): U16 ? =>
    _UnsignedPartialArithmetic.sub_partial[U16](this, y)?

  fun mul_partial(y: U16): U16 ? =>
    _UnsignedPartialArithmetic.mul_partial[U16](this, y)?

  fun div_partial(y: U16): U16 ? =>
    _UnsignedPartialArithmetic.div_partial[U16](this, y)?

  fun rem_partial(y: U16): U16 ? =>
    _UnsignedPartialArithmetic.rem_partial[U16](this, y)?

  fun divrem_partial(y: U16): (U16, U16) ? =>
    _UnsignedPartialArithmetic.divrem_partial[U16](this, y)?

primitive U32 is UnsignedInteger[U32]
  new create(value: U32) => value
  new from[A: (Number & Real[A] val)](a: A) => a.u32()

  new min_value() => 0
  new max_value() => 0xFFFF_FFFF

  fun next_pow2(): U32 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U32 => this
  fun bit_reverse(): U32 => @"llvm.bitreverse.i32"(this)
  fun bswap(): U32 => @"llvm.bswap.i32"(this)
  fun popcount(): U32 => @"llvm.ctpop.i32"(this)
  fun clz(): U32 => @"llvm.ctlz.i32"(this, false)
  fun ctz(): U32 => @"llvm.cttz.i32"(this, false)

  fun clz_unsafe(): U32 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i32"(this, true)

  fun ctz_unsafe(): U32 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i32"(this, true)

  fun bitwidth(): U32 => 32

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: U32): U32 => if this < y then this else y end
  fun max(y: U32): U32 => if this > y then this else y end

  fun addc(y: U32): (U32, Bool) =>
    @"llvm.uadd.with.overflow.i32"(this, y)

  fun subc(y: U32): (U32, Bool) =>
    @"llvm.usub.with.overflow.i32"(this, y)

  fun mulc(y: U32): (U32, Bool) =>
    @"llvm.umul.with.overflow.i32"(this, y)

  fun divc(y: U32): (U32, Bool) =>
    _UnsignedCheckedArithmetic.div_checked[U32](this, y)

  fun remc(y: U32): (U32, Bool) =>
    _UnsignedCheckedArithmetic.rem_checked[U32](this, y)

  fun add_partial(y: U32): U32 ? =>
    _UnsignedPartialArithmetic.add_partial[U32](this, y)?

  fun sub_partial(y: U32): U32 ? =>
    _UnsignedPartialArithmetic.sub_partial[U32](this, y)?

  fun mul_partial(y: U32): U32 ? =>
    _UnsignedPartialArithmetic.mul_partial[U32](this, y)?

  fun div_partial(y: U32): U32 ? =>
    _UnsignedPartialArithmetic.div_partial[U32](this, y)?

  fun rem_partial(y: U32): U32 ? =>
    _UnsignedPartialArithmetic.rem_partial[U32](this, y)?

  fun divrem_partial(y: U32): (U32, U32) ? =>
    _UnsignedPartialArithmetic.divrem_partial[U32](this, y)?

primitive U64 is UnsignedInteger[U64]
  new create(value: U64) => value
  new from[A: (Number & Real[A] val)](a: A) => a.u64()

  new min_value() => 0
  new max_value() => 0xFFFF_FFFF_FFFF_FFFF

  fun next_pow2(): U64 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U64 => this
  fun bit_reverse(): U64 => @"llvm.bitreverse.i64"(this)
  fun bswap(): U64 => @"llvm.bswap.i64"(this)
  fun popcount(): U64 => @"llvm.ctpop.i64"(this)
  fun clz(): U64 => @"llvm.ctlz.i64"(this, false)
  fun ctz(): U64 => @"llvm.cttz.i64"(this, false)

  fun clz_unsafe(): U64 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i64"(this, true)

  fun ctz_unsafe(): U64 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i64"(this, true)

  fun bitwidth(): U64 => 64

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: U64): U64 => if this < y then this else y end
  fun max(y: U64): U64 => if this > y then this else y end

  fun hash(): USize =>
    ifdef ilp32 then
      ((this >> 32).u32() xor this.u32()).hash()
    else
      usize().hash()
    end

  fun addc(y: U64): (U64, Bool) =>
    @"llvm.uadd.with.overflow.i64"(this, y)

  fun subc(y: U64): (U64, Bool) =>
    @"llvm.usub.with.overflow.i64"(this, y)

  fun mulc(y: U64): (U64, Bool) =>
    @"llvm.umul.with.overflow.i64"(this, y)

  fun divc(y: U64): (U64, Bool) =>
    _UnsignedCheckedArithmetic.div_checked[U64](this, y)

  fun remc(y: U64): (U64, Bool) =>
    _UnsignedCheckedArithmetic.rem_checked[U64](this, y)

  fun add_partial(y: U64): U64 ? =>
    _UnsignedPartialArithmetic.add_partial[U64](this, y)?

  fun sub_partial(y: U64): U64 ? =>
    _UnsignedPartialArithmetic.sub_partial[U64](this, y)?

  fun mul_partial(y: U64): U64 ? =>
    _UnsignedPartialArithmetic.mul_partial[U64](this, y)?

  fun div_partial(y: U64): U64 ? =>
    _UnsignedPartialArithmetic.div_partial[U64](this, y)?

  fun rem_partial(y: U64): U64 ? =>
    _UnsignedPartialArithmetic.rem_partial[U64](this, y)?

  fun divrem_partial(y: U64): (U64, U64) ? =>
    _UnsignedPartialArithmetic.divrem_partial[U64](this, y)?

primitive ULong is UnsignedInteger[ULong]
  new create(value: ULong) => value
  new from[A: (Number & Real[A] val)](a: A) => a.ulong()

  new min_value() => 0

  new max_value() =>
    ifdef ilp32 or llp64 then
      0xFFFF_FFFF
    else
      0xFFFF_FFFF_FFFF_FFFF
    end

  fun next_pow2(): ULong =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): ULong => this

  fun bit_reverse(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.bitreverse.i32"(this.u32()).ulong()
    else
      @"llvm.bitreverse.i64"(this.u64()).ulong()
    end

  fun bswap(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.bswap.i32"(this.u32()).ulong()
    else
      @"llvm.bswap.i64"(this.u64()).ulong()
    end

  fun popcount(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.ctpop.i32"(this.u32()).ulong()
    else
      @"llvm.ctpop.i64"(this.u64()).ulong()
    end

  fun clz(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.ctlz.i32"(this.u32(), false).ulong()
    else
      @"llvm.ctlz.i64"(this.u64(), false).ulong()
    end

  fun ctz(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.cttz.i32"(this.u32(), false).ulong()
    else
      @"llvm.cttz.i64"(this.u64(), false).ulong()
    end

  fun clz_unsafe(): ULong =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    ifdef ilp32 or llp64 then
      @"llvm.ctlz.i32"(this.u32(), true).ulong()
    else
      @"llvm.ctlz.i64"(this.u64(), true).ulong()
    end

  fun ctz_unsafe(): ULong =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    ifdef ilp32 or llp64 then
      @"llvm.cttz.i32"(this.u32(), false).ulong()
    else
      @"llvm.cttz.i64"(this.u64(), true).ulong()
    end

  fun bitwidth(): ULong => ifdef ilp32 or llp64 then 32 else 64 end

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: ULong): ULong => if this < y then this else y end
  fun max(y: ULong): ULong => if this > y then this else y end

  fun hash(): USize =>
    ifdef ilp32 or llp64 then
      u32().hash()
    else
      u64().hash()
    end

  fun addc(y: ULong): (ULong, Bool) =>
    ifdef ilp32 or llp64 then
      (let r, let o) =
        @"llvm.uadd.with.overflow.i32"(this.u32(), y.u32())
      (r.ulong(), o)
    else
      (let r, let o) =
        @"llvm.uadd.with.overflow.i64"(this.u64(), y.u64())
      (r.ulong(), o)
    end

  fun subc(y: ULong): (ULong, Bool) =>
    ifdef ilp32 or llp64 then
      (let r, let o) =
        @"llvm.usub.with.overflow.i32"(this.u32(), y.u32())
      (r.ulong(), o)
    else
      (let r, let o) =
        @"llvm.usub.with.overflow.i64"(this.u64(), y.u64())
      (r.ulong(), o)
    end

  fun mulc(y: ULong): (ULong, Bool) =>
    ifdef ilp32 or llp64 then
      (let r, let o) =
        @"llvm.umul.with.overflow.i32"(this.u32(), y.u32())
      (r.ulong(), o)
    else
      (let r, let o) =
        @"llvm.umul.with.overflow.i64"(this.u64(), y.u64())
      (r.ulong(), o)
    end

  fun divc(y: ULong): (ULong, Bool) =>
    _UnsignedCheckedArithmetic.div_checked[ULong](this, y)

  fun remc(y: ULong): (ULong, Bool) =>
    _UnsignedCheckedArithmetic.rem_checked[ULong](this, y)

  fun add_partial(y: ULong): ULong ? =>
    _UnsignedPartialArithmetic.add_partial[ULong](this, y)?

  fun sub_partial(y: ULong): ULong ? =>
    _UnsignedPartialArithmetic.sub_partial[ULong](this, y)?

  fun mul_partial(y: ULong): ULong ? =>
    _UnsignedPartialArithmetic.mul_partial[ULong](this, y)?

  fun div_partial(y: ULong): ULong ? =>
    _UnsignedPartialArithmetic.div_partial[ULong](this, y)?

  fun rem_partial(y: ULong): ULong ? =>
    _UnsignedPartialArithmetic.rem_partial[ULong](this, y)?

  fun divrem_partial(y: ULong): (ULong, ULong) ? =>
    _UnsignedPartialArithmetic.divrem_partial[ULong](this, y)?

primitive USize is UnsignedInteger[USize]
  new create(value: USize) => value
  new from[A: (Number & Real[A] val)](a: A) => a.usize()

  new min_value() => 0

  new max_value() =>
    ifdef ilp32 then
      0xFFFF_FFFF
    else
      0xFFFF_FFFF_FFFF_FFFF
    end

  fun next_pow2(): USize =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): USize => this

  fun bit_reverse(): USize =>
    ifdef ilp32 then
      @"llvm.bitreverse.i32"(this.u32()).usize()
    else
      @"llvm.bitreverse.i64"(this.u64()).usize()
    end

  fun bswap(): USize =>
    ifdef ilp32 then
      @"llvm.bswap.i32"(this.u32()).usize()
    else
      @"llvm.bswap.i64"(this.u64()).usize()
    end

  fun popcount(): USize =>
    ifdef ilp32 then
      @"llvm.ctpop.i32"(this.u32()).usize()
    else
      @"llvm.ctpop.i64"(this.u64()).usize()
    end

  fun clz(): USize =>
    ifdef ilp32 then
      @"llvm.ctlz.i32"(this.u32(), false).usize()
    else
      @"llvm.ctlz.i64"(this.u64(), false).usize()
    end

  fun ctz(): USize =>
    ifdef ilp32 then
      @"llvm.cttz.i32"(this.u32(), false).usize()
    else
      @"llvm.cttz.i64"(this.u64(), false).usize()
    end

  fun clz_unsafe(): USize =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    ifdef ilp32 then
      @"llvm.ctlz.i32"(this.u32(), true).usize()
    else
      @"llvm.ctlz.i64"(this.u64(), true).usize()
    end

  fun ctz_unsafe(): USize =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    ifdef ilp32 then
      @"llvm.cttz.i32"(this.u32(), true).usize()
    else
      @"llvm.cttz.i64"(this.u64(), true).usize()
    end

  fun bitwidth(): USize => ifdef ilp32 then 32 else 64 end

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: USize): USize => if this < y then this else y end
  fun max(y: USize): USize => if this > y then this else y end

  fun addc(y: USize): (USize, Bool) =>
    ifdef ilp32 then
      (let r, let o) =
        @"llvm.uadd.with.overflow.i32"(this.u32(), y.u32())
      (r.usize(), o)
    else
      (let r, let o) =
        @"llvm.uadd.with.overflow.i64"(this.u64(), y.u64())
      (r.usize(), o)
    end

  fun subc(y: USize): (USize, Bool) =>
    ifdef ilp32 then
      (let r, let o) =
        @"llvm.usub.with.overflow.i32"(this.u32(), y.u32())
      (r.usize(), o)
    else
      (let r, let o) =
        @"llvm.usub.with.overflow.i64"(this.u64(), y.u64())
      (r.usize(), o)
    end

  fun mulc(y: USize): (USize, Bool) =>
    ifdef ilp32 then
      (let r, let o) =
        @"llvm.umul.with.overflow.i32"(this.u32(), y.u32())
      (r.usize(), o)
    else
      (let r, let o) =
        @"llvm.umul.with.overflow.i64"(this.u64(), y.u64())
      (r.usize(), o)
    end

  fun divc(y: USize): (USize, Bool) =>
    _UnsignedCheckedArithmetic.div_checked[USize](this, y)

  fun remc(y: USize): (USize, Bool) =>
    _UnsignedCheckedArithmetic.rem_checked[USize](this, y)

  fun add_partial(y: USize): USize ? =>
    _UnsignedPartialArithmetic.add_partial[USize](this, y)?

  fun sub_partial(y: USize): USize ? =>
    _UnsignedPartialArithmetic.sub_partial[USize](this, y)?

  fun mul_partial(y: USize): USize ? =>
    _UnsignedPartialArithmetic.mul_partial[USize](this, y)?

  fun div_partial(y: USize): USize ? =>
    _UnsignedPartialArithmetic.div_partial[USize](this, y)?

  fun rem_partial(y: USize): USize ? =>
    _UnsignedPartialArithmetic.rem_partial[USize](this, y)?

  fun divrem_partial(y: USize): (USize, USize) ? =>
    _UnsignedPartialArithmetic.divrem_partial[USize](this, y)?

primitive U128 is UnsignedInteger[U128]
  new create(value: U128) => value
  new from[A: (Number & Real[A] val)](a: A) => a.u128()

  new min_value() => 0
  new max_value() => 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF

  fun next_pow2(): U128 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U128 => this
  fun bit_reverse(): U128 => @"llvm.bitreverse.i128"(this)
  fun bswap(): U128 => @"llvm.bswap.i128"(this)
  fun popcount(): U128 => @"llvm.ctpop.i128"(this)
  fun clz(): U128 => @"llvm.ctlz.i128"(this, false)
  fun ctz(): U128 => @"llvm.cttz.i128"(this, false)

  fun clz_unsafe(): U128 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i128"(this, true)

  fun ctz_unsafe(): U128 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i128"(this, true)

  fun bitwidth(): U128 => 128

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: U128): U128 => if this < y then this else y end
  fun max(y: U128): U128 => if this > y then this else y end

  fun hash(): USize =>
    ifdef ilp32 then
      ((this >> 96).u32() xor
      (this >> 64).u32() xor
      (this >> 32).u32() xor
      this.u32()).hash()
    else
      ((this >> 64).u64() xor this.u64()).hash()
    end

  fun hash64(): U64 =>
    ((this >> 64).u64() xor this.u64()).hash64()

  fun string(): String iso^ =>
    _ToString._u128(this, false)

  fun mul(y: U128): U128 =>
    ifdef native128 then
      this * y
    else
      let x_hi = (this >> 64).u64()
      let x_lo = this.u64()
      let y_hi = (y >> 64).u64()
      let y_lo = y.u64()

      let mask = U64(0x00000000FFFFFFFF)

      var lo = (x_lo and mask) * (y_lo and mask)
      var t = lo >> 32
      lo = lo and mask
      t = t + ((x_lo >> 32) * (y_lo and mask))
      lo = lo + ((t and mask) << 32)

      var hi = t >> 32
      t = lo >> 32
      lo = lo and mask
      t = t + ((y_lo >> 32) * (x_lo and mask))
      lo = lo + ((t and mask) << 32)
      hi = hi + (t >> 32)
      hi = hi + ((x_lo >> 32) * (y_lo >> 32))
      hi = hi + (x_hi * y_lo) + (x_lo * y_hi)

      (hi.u128() << 64) or lo.u128()
    end

  fun divrem(y: U128): (U128, U128) =>
    ifdef native128 then
      (this / y, this % y)
    else
      if y == 0 then
        return (0, 0)
      end

      var quot: U128 = 0
      var qbit: U128 = 1
      var num = this
      var den = y

      while den.i128() >= 0 do
        den = den << 1
        qbit = qbit << 1
      end

      while qbit != 0 do
        if den <= num then
          num = num - den
          quot = quot + qbit
        end

        den = den >> 1
        qbit = qbit >> 1
      end
      (quot, num)
    end

  fun div(y: U128): U128 =>
    ifdef native128 then
      this / y
    else
      (let q, let r) = divrem(y)
      q
    end

  fun rem(y: U128): U128 =>
    ifdef native128 then
      this % y
    else
      (let q, let r) = divrem(y)
      r
    end

  fun mul_unsafe(y: U128): U128 =>
    """
    Unsafe operation.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      this *~ y
    else
      this * y
    end

  fun divrem_unsafe(y: U128): (U128, U128) =>
    """
    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      (this *~ y, this /~ y)
    else
      divrem(y)
    end

  fun div_unsafe(y: U128): U128 =>
    """
    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      this /~ y
    else
      this / y
    end

  fun rem_unsafe(y: U128): U128 =>
    """
    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      this %~ y
    else
      this % y
    end

  fun f32(): F32 =>
    let v = f64()
    if v > F32.max_value().f64() then
      F32._inf(false)
    else
      v.f32()
    end

  fun f64(): F64 =>
    if this == 0 then
      return 0
    end

    var a = this
    let sd = bitwidth() - clz()
    var e = (sd - 1).u64()

    if sd > 53 then
      match sd
      | 54 => a = a << 1
      | 55 => None
      else
        a = (a >> (sd - 55)) or
          if (a and (-1 >> ((bitwidth() + 55) - sd))) != 0 then 1 else 0 end
      end

      if (a and 4) != 0 then
        a = a or 1
      end

      a = (a + 1) >> 2

      if (a and (1 << 53)) != 0 then
        a = a >> 1
        e = e + 1
      end
    else
      a = a << (53 - sd)
    end

    F64.from_bits(((e + 1023) << 52) or (a.u64() and 0xF_FFFF_FFFF_FFFF))

  fun f32_unsafe(): F32 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    f64_unsafe().f32_unsafe()

  fun f64_unsafe(): F64 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    f64()

  fun addc(y: U128): (U128, Bool) =>
    ifdef native128 then
      @"llvm.uadd.with.overflow.i128"(this, y)
    else
      let overflow = this > (max_value() - y)
      (this + y, overflow)
    end

  fun subc(y: U128): (U128, Bool) =>
    ifdef native128 then
      @"llvm.usub.with.overflow.i128"(this, y)
    else
      let overflow = this < y
      (this - y, overflow)
    end

  fun mulc(y: U128): (U128, Bool) =>
    ifdef native128 then
      @"llvm.umul.with.overflow.i128"(this, y)
    else
      let result = this * y
      let overflow = (this != 0) and ((result / this) != y)
      (result, overflow)
    end

  fun divc(y: U128): (U128, Bool) =>
    _UnsignedCheckedArithmetic.div_checked[U128](this, y)

  fun remc(y: U128): (U128, Bool) =>
    _UnsignedCheckedArithmetic.rem_checked[U128](this, y)

  fun add_partial(y: U128): U128 ? =>
    _UnsignedPartialArithmetic.add_partial[U128](this, y)?

  fun sub_partial(y: U128): U128 ? =>
    _UnsignedPartialArithmetic.sub_partial[U128](this, y)?

  fun mul_partial(y: U128): U128 ? =>
    _UnsignedPartialArithmetic.mul_partial[U128](this, y)?

  fun div_partial(y: U128): U128 ? =>
    _UnsignedPartialArithmetic.div_partial[U128](this, y)?

  fun rem_partial(y: U128): U128 ? =>
    _UnsignedPartialArithmetic.rem_partial[U128](this, y)?

  fun divrem_partial(y: U128): (U128, U128) ? =>
    _UnsignedPartialArithmetic.divrem_partial[U128](this, y)?

type Unsigned is (U8 | U16 | U32 | U64 | U128 | ULong | USize)

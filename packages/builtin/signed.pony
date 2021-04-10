use @"llvm.sadd.with.overflow.i8"[(I8, Bool)](a: I8, b: I8)
use @"llvm.sadd.with.overflow.i16"[(I16, Bool)](a: I16, b: I16)
use @"llvm.sadd.with.overflow.i32"[(I32, Bool)](a: I32, b: I32)
use @"llvm.sadd.with.overflow.i64"[(I64, Bool)](a: I64, b: I64)
use @"llvm.sadd.with.overflow.i128"[(I128, Bool)](a: I128, b: I128)
use @"llvm.ssub.with.overflow.i8"[(I8, Bool)](a: I8, b: I8)
use @"llvm.ssub.with.overflow.i16"[(I16, Bool)](a: I16, b: I16)
use @"llvm.ssub.with.overflow.i32"[(I32, Bool)](a: I32, b: I32)
use @"llvm.ssub.with.overflow.i64"[(I64, Bool)](a: I64, b: I64)
use @"llvm.ssub.with.overflow.i128"[(I128, Bool)](a: I128, b: I128)
use @"llvm.smul.with.overflow.i8"[(I8, Bool)](a: I8, b: I8)
use @"llvm.smul.with.overflow.i16"[(I16, Bool)](a: I16, b: I16)
use @"llvm.smul.with.overflow.i32"[(I32, Bool)](a: I32, b: I32)
use @"llvm.smul.with.overflow.i64"[(I64, Bool)](a: I64, b: I64)

primitive I8 is SignedInteger[I8, U8]
  new create(value: I8) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i8()

  new min_value() => -0x80
  new max_value() => 0x7F

  fun abs(): U8 => if this < 0 then (-this).u8() else this.u8() end
  fun bit_reverse(): I8 => @"llvm.bitreverse.i8"(this.u8()).i8()
  fun bswap(): I8 => this
  fun popcount(): U8 => @"llvm.ctpop.i8"(this.u8())
  fun clz(): U8 => @"llvm.ctlz.i8"(this.u8(), false)
  fun ctz(): U8 => @"llvm.cttz.i8"(this.u8(), false)

  fun clz_unsafe(): U8 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i8"(this.u8(), true)

  fun ctz_unsafe(): U8 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i8"(this.u8(), true)

  fun bitwidth(): U8 => 8

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: I8): I8 => if this < y then this else y end
  fun max(y: I8): I8 => if this > y then this else y end

  fun fld(y: I8): I8 =>
    _SignedArithmetic.fld[I8, U8](this, y)

  fun fld_unsafe(y: I8): I8 =>
    _SignedUnsafeArithmetic.fld_unsafe[I8, U8](this, y)

  fun mod(y: I8): I8 =>
    _SignedArithmetic.mod[I8, U8](this, y)

  fun mod_unsafe(y: I8): I8 =>
    _SignedUnsafeArithmetic.mod_unsafe[I8, U8](this, y)

  fun addc(y: I8): (I8, Bool) =>
    @"llvm.sadd.with.overflow.i8"(this, y)

  fun subc(y: I8): (I8, Bool) =>
    @"llvm.ssub.with.overflow.i8"(this, y)

  fun mulc(y: I8): (I8, Bool) =>
    @"llvm.smul.with.overflow.i8"(this, y)

  fun divc(y: I8): (I8, Bool) =>
    _SignedCheckedArithmetic.div_checked[I8, U8](this, y)

  fun remc(y: I8): (I8, Bool) =>
    _SignedCheckedArithmetic.rem_checked[I8, U8](this, y)

  fun fldc(y: I8): (I8, Bool) =>
    _SignedCheckedArithmetic.fld_checked[I8, U8](this, y)

  fun modc(y: I8): (I8, Bool) =>
    _SignedCheckedArithmetic.mod_checked[I8, U8](this, y)

  fun add_partial(y: I8): I8 ? =>
    _SignedPartialArithmetic.add_partial[I8](this, y)?

  fun sub_partial(y: I8): I8 ? =>
    _SignedPartialArithmetic.sub_partial[I8](this, y)?

  fun mul_partial(y: I8): I8 ? =>
    _SignedPartialArithmetic.mul_partial[I8](this, y)?

  fun div_partial(y: I8): I8 ? =>
    _SignedPartialArithmetic.div_partial[I8, U8](this, y)?

  fun rem_partial(y: I8): I8 ? =>
    _SignedPartialArithmetic.rem_partial[I8, U8](this, y)?

  fun divrem_partial(y: I8): (I8, I8) ? =>
    _SignedPartialArithmetic.divrem_partial[I8, U8](this, y)?

  fun fld_partial(y: I8): I8 ? =>
    _SignedPartialArithmetic.fld_partial[I8, U8](this, y)?

  fun mod_partial(y: I8): I8 ? =>
    _SignedPartialArithmetic.mod_partial[I8, U8](this, y)?

primitive I16 is SignedInteger[I16, U16]
  new create(value: I16) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i16()

  new min_value() => -0x8000
  new max_value() => 0x7FFF

  fun abs(): U16 => if this < 0 then (-this).u16() else this.u16() end
  fun bit_reverse(): I16 => @"llvm.bitreverse.i16"(this.u16()).i16()
  fun bswap(): I16 => @"llvm.bswap.i16"(this.u16()).i16()
  fun popcount(): U16 => @"llvm.ctpop.i16"(this.u16())
  fun clz(): U16 => @"llvm.ctlz.i16"(this.u16(), false)
  fun ctz(): U16 => @"llvm.cttz.i16"(this.u16(), false)

  fun clz_unsafe(): U16 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i16"(this.u16(), true)

  fun ctz_unsafe(): U16 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i16"(this.u16(), true)

  fun bitwidth(): U16 => 16

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: I16): I16 => if this < y then this else y end
  fun max(y: I16): I16 => if this > y then this else y end

  fun fld(y: I16): I16 =>
    _SignedArithmetic.fld[I16, U16](this, y)

  fun fld_unsafe(y: I16): I16 =>
    _SignedUnsafeArithmetic.fld_unsafe[I16, U16](this, y)

  fun mod(y: I16): I16 =>
    _SignedArithmetic.mod[I16, U16](this, y)

  fun mod_unsafe(y: I16): I16 =>
    _SignedUnsafeArithmetic.mod_unsafe[I16, U16](this, y)

  fun addc(y: I16): (I16, Bool) =>
    @"llvm.sadd.with.overflow.i16"(this, y)

  fun subc(y: I16): (I16, Bool) =>
    @"llvm.ssub.with.overflow.i16"(this, y)

  fun mulc(y: I16): (I16, Bool) =>
    @"llvm.smul.with.overflow.i16"(this, y)

  fun divc(y: I16): (I16, Bool) =>
    _SignedCheckedArithmetic.div_checked[I16, U16](this, y)

  fun remc(y: I16): (I16, Bool) =>
    _SignedCheckedArithmetic.rem_checked[I16, U16](this, y)

  fun fldc(y: I16): (I16, Bool) =>
    _SignedCheckedArithmetic.fld_checked[I16, U16](this, y)

  fun modc(y: I16): (I16, Bool) =>
    _SignedCheckedArithmetic.mod_checked[I16, U16](this, y)

  fun add_partial(y: I16): I16 ? =>
    _SignedPartialArithmetic.add_partial[I16](this, y)?

  fun sub_partial(y: I16): I16 ? =>
    _SignedPartialArithmetic.sub_partial[I16](this, y)?

  fun mul_partial(y: I16): I16 ? =>
    _SignedPartialArithmetic.mul_partial[I16](this, y)?

  fun div_partial(y: I16): I16 ? =>
    _SignedPartialArithmetic.div_partial[I16, U16](this, y)?

  fun rem_partial(y: I16): I16 ? =>
    _SignedPartialArithmetic.rem_partial[I16, U16](this, y)?

  fun divrem_partial(y: I16): (I16, I16) ? =>
    _SignedPartialArithmetic.divrem_partial[I16, U16](this, y)?

  fun fld_partial(y: I16): I16 ? =>
    _SignedPartialArithmetic.fld_partial[I16, U16](this, y)?

  fun mod_partial(y: I16): I16 ? =>
    _SignedPartialArithmetic.mod_partial[I16, U16](this, y)?


primitive I32 is SignedInteger[I32, U32]
  new create(value: I32) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i32()

  new min_value() => -0x8000_0000
  new max_value() => 0x7FFF_FFFF

  fun abs(): U32 => if this < 0 then (-this).u32() else this.u32() end
  fun bit_reverse(): I32 => @"llvm.bitreverse.i32"(this.u32()).i32()
  fun bswap(): I32 => @"llvm.bswap.i32"(this.u32()).i32()
  fun popcount(): U32 => @"llvm.ctpop.i32"(this.u32())
  fun clz(): U32 => @"llvm.ctlz.i32"(this.u32(), false)
  fun ctz(): U32 => @"llvm.cttz.i32"(this.u32(), false)

  fun clz_unsafe(): U32 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i32"(this.u32(), true)

  fun ctz_unsafe(): U32 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i32"(this.u32(), true)

  fun bitwidth(): U32 => 32

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: I32): I32 => if this < y then this else y end
  fun max(y: I32): I32 => if this > y then this else y end

  fun fld(y: I32): I32 =>
    _SignedArithmetic.fld[I32, U32](this, y)

  fun fld_unsafe(y: I32): I32 =>
    _SignedUnsafeArithmetic.fld_unsafe[I32, U32](this, y)

  fun mod(y: I32): I32 =>
    _SignedArithmetic.mod[I32, U32](this, y)

  fun mod_unsafe(y: I32): I32 =>
    _SignedUnsafeArithmetic.mod_unsafe[I32, U32](this, y)

  fun addc(y: I32): (I32, Bool) =>
    @"llvm.sadd.with.overflow.i32"(this, y)

  fun subc(y: I32): (I32, Bool) =>
    @"llvm.ssub.with.overflow.i32"(this, y)

  fun mulc(y: I32): (I32, Bool) =>
    @"llvm.smul.with.overflow.i32"(this, y)

  fun divc(y: I32): (I32, Bool) =>
    _SignedCheckedArithmetic.div_checked[I32, U32](this, y)

  fun remc(y: I32): (I32, Bool) =>
    _SignedCheckedArithmetic.rem_checked[I32, U32](this, y)

  fun fldc(y: I32): (I32, Bool) =>
    _SignedCheckedArithmetic.fld_checked[I32, U32](this, y)

  fun modc(y: I32): (I32, Bool) =>
    _SignedCheckedArithmetic.mod_checked[I32, U32](this, y)

  fun add_partial(y: I32): I32 ? =>
    _SignedPartialArithmetic.add_partial[I32](this, y)?

  fun sub_partial(y: I32): I32 ? =>
    _SignedPartialArithmetic.sub_partial[I32](this, y)?

  fun mul_partial(y: I32): I32 ? =>
    _SignedPartialArithmetic.mul_partial[I32](this, y)?

  fun div_partial(y: I32): I32 ? =>
    _SignedPartialArithmetic.div_partial[I32, U32](this, y)?

  fun rem_partial(y: I32): I32 ? =>
    _SignedPartialArithmetic.rem_partial[I32, U32](this, y)?

  fun divrem_partial(y: I32): (I32, I32) ? =>
    _SignedPartialArithmetic.divrem_partial[I32, U32](this, y)?

  fun fld_partial(y: I32): I32 ? =>
    _SignedPartialArithmetic.fld_partial[I32, U32](this, y)?

  fun mod_partial(y: I32): I32 ? =>
    _SignedPartialArithmetic.mod_partial[I32, U32](this, y)?

primitive I64 is SignedInteger[I64, U64]
  new create(value: I64) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i64()

  new min_value() => -0x8000_0000_0000_0000
  new max_value() => 0x7FFF_FFFF_FFFF_FFFF

  fun abs(): U64 => if this < 0 then (-this).u64() else this.u64() end
  fun bit_reverse(): I64 => @"llvm.bitreverse.i64"(this.u64()).i64()
  fun bswap(): I64 => @"llvm.bswap.i64"(this.u64()).i64()
  fun popcount(): U64 => @"llvm.ctpop.i64"(this.u64())
  fun clz(): U64 => @"llvm.ctlz.i64"(this.u64(), false)
  fun ctz(): U64 => @"llvm.cttz.i64"(this.u64(), false)

  fun clz_unsafe(): U64 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i64"(this.u64(), true)

  fun ctz_unsafe(): U64 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i64"(this.u64(), true)

  fun bitwidth(): U64 => 64

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: I64): I64 => if this < y then this else y end
  fun max(y: I64): I64 => if this > y then this else y end

  fun fld(y: I64): I64 =>
    _SignedArithmetic.fld[I64, U64](this, y)

  fun fld_unsafe(y: I64): I64 =>
    _SignedUnsafeArithmetic.fld_unsafe[I64, U64](this, y)

  fun mod(y: I64): I64 =>
    _SignedArithmetic.mod[I64, U64](this, y)

  fun mod_unsafe(y: I64): I64 =>
    _SignedUnsafeArithmetic.mod_unsafe[I64, U64](this, y)

  fun hash(): USize => u64().hash()

  fun addc(y: I64): (I64, Bool) =>
    @"llvm.sadd.with.overflow.i64"(this, y)

  fun subc(y: I64): (I64, Bool) =>
    @"llvm.ssub.with.overflow.i64"(this, y)

  fun mulc(y: I64): (I64, Bool) =>
    _SignedCheckedArithmetic._mul_checked[U64, I64](this, y)

  fun divc(y: I64): (I64, Bool) =>
    _SignedCheckedArithmetic.div_checked[I64, U64](this, y)

  fun remc(y: I64): (I64, Bool) =>
    _SignedCheckedArithmetic.rem_checked[I64, U64](this, y)

  fun fldc(y: I64): (I64, Bool) =>
    _SignedCheckedArithmetic.fld_checked[I64, U64](this, y)

  fun modc(y: I64): (I64, Bool) =>
    _SignedCheckedArithmetic.mod_checked[I64, U64](this, y)

  fun add_partial(y: I64): I64 ? =>
    _SignedPartialArithmetic.add_partial[I64](this, y)?

  fun sub_partial(y: I64): I64 ? =>
    _SignedPartialArithmetic.sub_partial[I64](this, y)?

  fun mul_partial(y: I64): I64 ? =>
    _SignedPartialArithmetic.mul_partial[I64](this, y)?

  fun div_partial(y: I64): I64 ? =>
    _SignedPartialArithmetic.div_partial[I64, U64](this, y)?

  fun rem_partial(y: I64): I64 ? =>
    _SignedPartialArithmetic.rem_partial[I64, U64](this, y)?

  fun divrem_partial(y: I64): (I64, I64) ? =>
    _SignedPartialArithmetic.divrem_partial[I64, U64](this, y)?

  fun fld_partial(y: I64): I64 ? =>
    _SignedPartialArithmetic.fld_partial[I64, U64](this, y)?

  fun mod_partial(y: I64): I64 ? =>
    _SignedPartialArithmetic.mod_partial[I64, U64](this, y)?

primitive ILong is SignedInteger[ILong, ULong]
  new create(value: ILong) => value
  new from[A: (Number & Real[A] val)](a: A) => a.ilong()

  new min_value() =>
    ifdef ilp32 or llp64 then
      -0x8000_0000
    else
      -0x8000_0000_0000_0000
    end

  new max_value() =>
    ifdef ilp32 or llp64 then
      0x7FFF_FFFF
    else
      0x7FFF_FFFF_FFFF_FFFF
    end

  fun abs(): ULong => if this < 0 then (-this).ulong() else this.ulong() end

  fun bit_reverse(): ILong =>
    ifdef ilp32 or llp64 then
      @"llvm.bitreverse.i32"(this.u32()).ilong()
    else
      @"llvm.bitreverse.i64"(this.u64()).ilong()
    end

  fun bswap(): ILong =>
    ifdef ilp32 or llp64 then
      @"llvm.bswap.i32"(this.u32()).ilong()
    else
      @"llvm.bswap.i64"(this.u64()).ilong()
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
    ifdef ilp32 or llp64 then
      @"llvm.ctlz.i32"(this.u32(), true).ulong()
    else
      @"llvm.ctlz.i64"(this.u64(), true).ulong()
    end

  fun ctz_unsafe(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.cttz.i32"(this.u32(), false).ulong()
    else
      @"llvm.cttz.i64"(this.u64(), true).ulong()
    end

  fun bitwidth(): ULong => ifdef ilp32 or llp64 then 32 else 64 end

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: ILong): ILong => if this < y then this else y end
  fun max(y: ILong): ILong => if this > y then this else y end

  fun fld(y: ILong): ILong =>
    _SignedArithmetic.fld[ILong, ULong](this, y)

  fun fld_unsafe(y: ILong): ILong =>
    _SignedUnsafeArithmetic.fld_unsafe[ILong, ULong](this, y)

  fun mod(y: ILong): ILong =>
    _SignedArithmetic.mod[ILong, ULong](this, y)

  fun mod_unsafe(y: ILong): ILong =>
    _SignedUnsafeArithmetic.mod_unsafe[ILong, ULong](this, y)

  fun hash(): USize => ulong().hash()

  fun addc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      (let r, let o) =
        @"llvm.sadd.with.overflow.i32"(this.i32(), y.i32())
      (r.ilong(), o)
    else
      (let r, let o) =
        @"llvm.sadd.with.overflow.i64"(this.i64(), y.i64())
      (r.ilong(), o)
    end

  fun subc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      (let r, let o) =
        @"llvm.ssub.with.overflow.i32"(this.i32(), y.i32())
      (r.ilong(), o)
    else
      (let r, let o) =
        @"llvm.ssub.with.overflow.i64"(this.i64(), y.i64())
      (r.ilong(), o)
    end

  fun mulc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      (let r, let o) =
        @"llvm.smul.with.overflow.i32"(this.i32(), y.i32())
      (r.ilong(), o)
    else
      _SignedCheckedArithmetic._mul_checked[ULong, ILong](this, y)
    end

  fun divc(y: ILong): (ILong, Bool) =>
    _SignedCheckedArithmetic.div_checked[ILong, ULong](this, y)

  fun remc(y: ILong): (ILong, Bool) =>
    _SignedCheckedArithmetic.rem_checked[ILong, ULong](this, y)

  fun fldc(y: ILong): (ILong, Bool) =>
    _SignedCheckedArithmetic.fld_checked[ILong, ULong](this, y)

  fun modc(y: ILong): (ILong, Bool) =>
    _SignedCheckedArithmetic.mod_checked[ILong, ULong](this, y)

  fun add_partial(y: ILong): ILong ? =>
    _SignedPartialArithmetic.add_partial[ILong](this, y)?

  fun sub_partial(y: ILong): ILong ? =>
    _SignedPartialArithmetic.sub_partial[ILong](this, y)?

  fun mul_partial(y: ILong): ILong ? =>
    _SignedPartialArithmetic.mul_partial[ILong](this, y)?

  fun div_partial(y: ILong): ILong ? =>
    _SignedPartialArithmetic.div_partial[ILong, ULong](this, y)?

  fun rem_partial(y: ILong): ILong ? =>
    _SignedPartialArithmetic.rem_partial[ILong, ULong](this, y)?

  fun divrem_partial(y: ILong): (ILong, ILong) ? =>
    _SignedPartialArithmetic.divrem_partial[ILong, ULong](this, y)?

  fun fld_partial(y: ILong): ILong ? =>
    _SignedPartialArithmetic.fld_partial[ILong, ULong](this, y)?

  fun mod_partial(y: ILong): ILong ? =>
    _SignedPartialArithmetic.mod_partial[ILong, ULong](this, y)?

primitive ISize is SignedInteger[ISize, USize]
  new create(value: ISize) => value
  new from[A: (Number & Real[A] val)](a: A) => a.isize()

  new min_value() =>
    ifdef ilp32 then
      -0x8000_0000
    else
      -0x8000_0000_0000_0000
    end

  new max_value() =>
    ifdef ilp32 then
      0x7FFF_FFFF
    else
      0x7FFF_FFFF_FFFF_FFFF
    end

  fun abs(): USize => if this < 0 then (-this).usize() else this.usize() end

  fun bit_reverse(): ISize =>
    ifdef ilp32 then
      @"llvm.bitreverse.i32"(this.u32()).isize()
    else
      @"llvm.bitreverse.i64"(this.u64()).isize()
    end

  fun bswap(): ISize =>
    ifdef ilp32 then
      @"llvm.bswap.i32"(this.u32()).isize()
    else
      @"llvm.bswap.i64"(this.u64()).isize()
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
    ifdef ilp32 then
      @"llvm.ctlz.i32"(this.u32(), true).usize()
    else
      @"llvm.ctlz.i64"(this.u64(), true).usize()
    end

  fun ctz_unsafe(): USize =>
    ifdef ilp32 then
      @"llvm.cttz.i32"(this.u32(), true).usize()
    else
      @"llvm.cttz.i64"(this.u64(), true).usize()
    end

  fun bitwidth(): USize => ifdef ilp32 then 32 else 64 end

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: ISize): ISize => if this < y then this else y end
  fun max(y: ISize): ISize => if this > y then this else y end

  fun fld(y: ISize): ISize =>
    _SignedArithmetic.fld[ISize, USize](this, y)

  fun fld_unsafe(y: ISize): ISize =>
    _SignedUnsafeArithmetic.fld_unsafe[ISize, USize](this, y)

  fun mod(y: ISize): ISize =>
    _SignedArithmetic.mod[ISize, USize](this, y)

  fun mod_unsafe(y: ISize): ISize =>
    _SignedUnsafeArithmetic.mod_unsafe[ISize, USize](this, y)

  fun addc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      (let r, let o) =
        @"llvm.sadd.with.overflow.i32"(this.i32(), y.i32())
      (r.isize(), o)
    else
      (let r, let o) =
        @"llvm.sadd.with.overflow.i64"(this.i64(), y.i64())
      (r.isize(), o)
    end

  fun subc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      (let r, let o) =
        @"llvm.ssub.with.overflow.i32"(this.i32(), y.i32())
      (r.isize(), o)
    else
      (let r, let o) =
        @"llvm.ssub.with.overflow.i64"(this.i64(), y.i64())
      (r.isize(), o)
    end

  fun mulc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      (let r, let o) =
        @"llvm.smul.with.overflow.i32"(this.i32(), y.i32())
      (r.isize(), o)
    else
      _SignedCheckedArithmetic._mul_checked[USize, ISize](this, y)
    end

  fun divc(y: ISize): (ISize, Bool) =>
    _SignedCheckedArithmetic.div_checked[ISize, USize](this, y)

  fun remc(y: ISize): (ISize, Bool) =>
    _SignedCheckedArithmetic.rem_checked[ISize, USize](this, y)

  fun fldc(y: ISize): (ISize, Bool) =>
    _SignedCheckedArithmetic.fld_checked[ISize, USize](this, y)

  fun modc(y: ISize): (ISize, Bool) =>
    _SignedCheckedArithmetic.mod_checked[ISize, USize](this, y)

  fun add_partial(y: ISize): ISize ? =>
    _SignedPartialArithmetic.add_partial[ISize](this, y)?

  fun sub_partial(y: ISize): ISize ? =>
    _SignedPartialArithmetic.sub_partial[ISize](this, y)?

  fun mul_partial(y: ISize): ISize ? =>
    _SignedPartialArithmetic.mul_partial[ISize](this, y)?

  fun div_partial(y: ISize): ISize ? =>
    _SignedPartialArithmetic.div_partial[ISize, USize](this, y)?

  fun rem_partial(y: ISize): ISize ? =>
    _SignedPartialArithmetic.rem_partial[ISize, USize](this, y)?

  fun divrem_partial(y: ISize): (ISize, ISize) ? =>
    _SignedPartialArithmetic.divrem_partial[ISize, USize](this, y)?

  fun fld_partial(y: ISize): ISize ? =>
    _SignedPartialArithmetic.fld_partial[ISize, USize](this, y)?

  fun mod_partial(y: ISize): ISize ? =>
    _SignedPartialArithmetic.mod_partial[ISize, USize](this, y)?

primitive I128 is SignedInteger[I128, U128]
  new create(value: I128) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i128()

  new min_value() => -0x8000_0000_0000_0000_0000_0000_0000_0000
  new max_value() => 0x7FFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF

  fun abs(): U128 => if this < 0 then (-this).u128() else this.u128() end
  fun bit_reverse(): I128 => @"llvm.bitreverse.i128"(this.u128()).i128()
  fun bswap(): I128 => @"llvm.bswap.i128"(this.u128()).i128()
  fun popcount(): U128 => @"llvm.ctpop.i128"(this.u128())
  fun clz(): U128 => @"llvm.ctlz.i128"(this.u128(), false)
  fun ctz(): U128 => @"llvm.cttz.i128"(this.u128(), false)

  fun clz_unsafe(): U128 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i128"(this.u128(), true)

  fun ctz_unsafe(): U128 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i128"(this.u128(), true)

  fun bitwidth(): U128 => 128

  fun bytewidth(): USize => bitwidth().usize() / 8

  fun min(y: I128): I128 => if this < y then this else y end
  fun max(y: I128): I128 => if this > y then this else y end

  fun fld(y: I128): I128 => _SignedArithmetic.fld[I128, U128](this, y)

  fun fld_unsafe(y: I128): I128 =>
    _SignedUnsafeArithmetic.fld_unsafe[I128, U128](this, y)

  fun mod(y: I128): I128 =>
    _SignedArithmetic.mod[I128, U128](this, y)

  fun mod_unsafe(y: I128): I128 =>
    _SignedUnsafeArithmetic.mod_unsafe[I128, U128](this, y)

  fun hash(): USize => u128().hash()
  fun hash64(): U64 => u128().hash64()

  fun string(): String iso^ =>
    _ToString._u128(abs().u128(), this < 0)

  fun mul(y: I128): I128 =>
    (u128() * y.u128()).i128()

  fun divrem(y: I128): (I128, I128) =>
    ifdef native128 then
      (this / y, this % y)
    else
      if y == 0 then
        return (0, 0)
      end

      var num: I128 = if this >= 0 then this else -this end
      var den: I128 = if y >= 0 then y else -y end

      (let q, let r) = num.u128().divrem(den.u128())
      (var q', var r') = (q.i128(), r.i128())

      if this < 0 then
        r' = -r'

        if y > 0 then
          q' = -q'
        end
      elseif y < 0 then
        q' = -q'
      end

      (q', r')
    end

  fun div(y: I128): I128 =>
    ifdef native128 then
      this / y
    else
      (let q, let r) = divrem(y)
      q
    end

  fun rem(y: I128): I128 =>
    ifdef native128 then
      this % y
    else
      (let q, let r) = divrem(y)
      r
    end

  fun mul_unsafe(y: I128): I128 =>
    """
    Unsafe operation.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      this *~ y
    else
      this * y
    end

  fun divrem_unsafe(y: I128): (I128, I128) =>
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

  fun div_unsafe(y: I128): I128 =>
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

  fun rem_unsafe(y: I128): I128 =>
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
    f64().f32()

  fun f64(): F64 =>
    if this < 0 then
      -(-u128()).f64()
    else
      u128().f64()
    end

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

  fun addc(y: I128): (I128, Bool) =>
    ifdef native128 then
      @"llvm.sadd.with.overflow.i128"(this, y)
    else
      let overflow =
        if y > 0 then
          (this > (max_value() - y))
        else
          (this < (min_value() - y))
        end

      (this + y, overflow)
    end

  fun subc(y: I128): (I128, Bool) =>
    ifdef native128 then
      @"llvm.ssub.with.overflow.i128"(this, y)
    else
      let overflow =
        if y > 0 then
          (this < (min_value() + y))
        else
          (this > (max_value() + y))
        end

      (this - y, overflow)
    end

  fun mulc(y: I128): (I128, Bool) =>
    // using llvm.smul.with.overflow.i128 would require to link
    // llvm compiler-rt where the function implementing it lives: https://github.com/llvm-mirror/compiler-rt/blob/master/lib/builtins/muloti4.c
    // See this bug for reference:
    // the following implementation is more or less exactly was __muloti4 is
    // doing
    _SignedCheckedArithmetic._mul_checked[U128, I128](this, y)

  fun divc(y: I128): (I128, Bool) =>
    _SignedCheckedArithmetic.div_checked[I128, U128](this, y)

  fun remc(y: I128): (I128, Bool) =>
    _SignedCheckedArithmetic.rem_checked[I128, U128](this, y)

  fun fldc(y: I128): (I128, Bool) =>
    _SignedCheckedArithmetic.fld_checked[I128, U128](this, y)

  fun modc(y: I128): (I128, Bool) =>
    _SignedCheckedArithmetic.mod_checked[I128, U128](this, y)

  fun add_partial(y: I128): I128 ? =>
    _SignedPartialArithmetic.add_partial[I128](this, y)?

  fun sub_partial(y: I128): I128 ? =>
    _SignedPartialArithmetic.sub_partial[I128](this, y)?

  fun mul_partial(y: I128): I128 ? =>
    _SignedPartialArithmetic.mul_partial[I128](this, y)?

  fun div_partial(y: I128): I128 ? =>
    _SignedPartialArithmetic.div_partial[I128, U128](this, y)?

  fun rem_partial(y: I128): I128 ? =>
    _SignedPartialArithmetic.rem_partial[I128, U128](this, y)?

  fun divrem_partial(y: I128): (I128, I128) ? =>
    _SignedPartialArithmetic.divrem_partial[I128, U128](this, y)?

  fun fld_partial(y: I128): I128 ? =>
    _SignedPartialArithmetic.fld_partial[I128, U128](this, y)?

  fun mod_partial(y: I128): I128 ? =>
    _SignedPartialArithmetic.mod_partial[I128, U128](this, y)?

type Signed is (I8 | I16 | I32 | I64 | I128 | ILong | ISize)






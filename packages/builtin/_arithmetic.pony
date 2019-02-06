primitive _SignedArithmetic
  fun fld[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): T =>
    if (y == T.from[U8](0)) or ((x == T.min_value()) and (y == T.from[I8](-1))) then
      T.from[U8](0)
    else
      _SignedUnsafeArithmetic.fld_unsafe[T, U](x, y)
    end

  fun mod[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): T =>
    // stupid special case for division edge cases
    if (y == T.from[U8](0)) or ((x == T.min_value()) and (y == T.from[I8](-1))) then
      T.from[U8](0)
    else
      _SignedUnsafeArithmetic.mod_unsafe[T, U](x, y)
    end

primitive _SignedUnsafeArithmetic
  fun fld_unsafe[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): T =>
    let div_res = x /~ y
    if ((x xor y) < T.from[U8](0)) and ((div_res *~ y) != x) then
      div_res - T.from[U8](1)
    else
      div_res
    end

  fun mod_unsafe[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): T =>
    x -~ (fld_unsafe[T, U](x, y) *~ y)


primitive _UnsignedCheckedArithmetic
  fun div_checked[T: UnsignedInteger[T] val](x: T, y: T): (T, Bool) =>
    (x / y, (y == T.from[U8](0)))

  fun rem_checked[T: UnsignedInteger[T] val](x: T, y: T): (T, Bool) =>
    (x % y, y == T.from[U8](0))

  fun fld_checked[T: UnsignedInteger[T] val](x: T, y: T): (T, Bool) =>
    div_checked[T](x, y)

  fun mod_checked[T: UnsignedInteger[T] val](x: T, y: T): (T, Bool) =>
    rem_checked[T](x, y)


primitive _SignedCheckedArithmetic
  fun _mul_checked[U: UnsignedInteger[U] val, T: (Signed & SignedInteger[T, U] val)](x: T, y: T): (T, Bool) =>
    """
    basically exactly what the runtime functions __muloti4, mulodi4 etc. are doing
    and roughly as fast as these.

    Additionally on (at least some) 32 bit systems, the runtime function for checked 64 bit integer addition __mulodi4 is not available.
    So we shouldn't use: `@"llvm.smul.with.overflow.i64"[(I64, Bool)](this, y)`

    Also see https://bugs.llvm.org/show_bug.cgi?id=14469

    That's basically why we rolled our own.
    """
    let result = x * y
    if x == T.min_value() then
      return (result, (y != T.from[I8](0)) and (y != T.from[I8](1)))
    end
    if y == T.min_value() then
      return (result, (x != T.from[I8](0)) and (x != T.from[I8](1)))
    end
    let x_neg = x >> (x.bitwidth() - U.from[U8](1))
    let x_abs = (x xor x_neg) - x_neg
    let y_neg = y >> (x.bitwidth() - U.from[U8](1))
    let y_abs = (y xor y_neg) - y_neg

    if ((x_abs < T.from[I8](2)) or (y_abs < T.from[I8](2))) then
      return (result, false)
    end
    if (x_neg == y_neg) then
      (result, (x_abs > (T.max_value() / y_abs)))
    else
      (result, (x_abs > (T.min_value() / -y_abs)))
    end


  fun div_checked[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): (T, Bool) =>
    (x / y, (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())))

  fun rem_checked[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): (T, Bool) =>
    (x % y, (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())))

  fun fld_checked[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): (T, Bool) =>
    (x.fld(y), (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())))

  fun mod_checked[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): (T, Bool) =>
    (x.mod(y), (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())))


trait _PartialArithmetic
  fun add_partial[T: (Integer[T] val & Int)](x: T, y: T): T? =>
    (let r: T, let overflow: Bool) = x.addc(y)
    if overflow then error else r end

  fun sub_partial[T: (Integer[T] val & Int)](x: T, y: T): T? =>
    (let r: T, let overflow: Bool) = x.subc(y)
    if overflow then error else r end

  fun mul_partial[T: (Integer[T] val & Int)](x: T, y: T): T? =>
    (let r: T, let overflow: Bool) = x.mulc(y)
    if overflow then error else r end

primitive _UnsignedPartialArithmetic is _PartialArithmetic
  fun div_partial[T: UnsignedInteger[T] val](x: T, y: T): T? =>
    if (y == T.from[U8](0)) then
      error
    else
      x /~ y
    end

  fun rem_partial[T: UnsignedInteger[T] val](x: T, y: T): T? =>
    if (y == T.from[U8](0)) then
      error
    else
      x %~ y
    end

  fun divrem_partial[T: UnsignedInteger[T] val](x: T, y: T): (T, T)? =>
    if (y == T.from[U8](0)) then
      error
    else
      (x /~ y, x %~ y)
    end

  fun fld_partial[T: UnsignedInteger[T] val](x: T, y: T): T? =>
    if (y == T.from[U8](0)) then
      error
    else
      x.fld(y)
    end

  fun mod_partial[T: UnsignedInteger[T] val](x: T, y: T): T? =>
    if (y == T.from[U8](0)) then
      error
    else
      x.mod(y)
    end

primitive _SignedPartialArithmetic is _PartialArithmetic
  fun div_partial[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): T? =>
    if (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())) then
      error
    else
      x /~ y
    end

  fun rem_partial[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): T? =>
    if (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())) then
      error
    else
      x %~ y
    end

  fun divrem_partial[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): (T, T)? =>
    if (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())) then
      error
    else
      (x /~ y, x %~ y)
    end

  fun fld_partial[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): T? =>
    if (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())) then
      error
    else
      x.fld(y)
    end

  fun mod_partial[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T, y: T): T? =>
    if (y == T.from[I8](0)) or ((y == T.from[I8](I8(-1))) and (x == T.min_value())) then
      error
    else
      x.mod(y)
    end

  fun neg_partial[T: (SignedInteger[T, U] val & Signed), U: UnsignedInteger[U] val](x: T): T? =>
    if x == T.min_value() then
      error
    else
      -~x
    end

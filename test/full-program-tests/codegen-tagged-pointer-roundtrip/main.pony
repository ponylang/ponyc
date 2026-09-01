use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    var ok = true

    ok = ok and _check_u32()
    ok = ok and _check_i32()
    ok = ok and _check_bool()
    ok = ok and _check_u8()
    ok = ok and _check_i16()
    ok = ok and _check_u16()
    ok = ok and _check_i8_negative()
    ok = ok and _check_f32()
    ok = ok and _check_identity()
    ok = ok and _check_stringable()

    @pony_exitcode(I32(if ok then 1 else 0 end))

  fun _check_u32(): Bool =>
    let x: (String | U32) = U32(42)
    match x
    | let n: U32 => n == 42
    else false
    end

  fun _check_i32(): Bool =>
    let x: (String | I32) = I32(-1)
    match x
    | let n: I32 => n == -1
    else false
    end

  fun _check_bool(): Bool =>
    let x: (String | Bool) = true
    match x
    | let b: Bool => b == true
    else false
    end

  fun _check_u8(): Bool =>
    let x: (String | U8) = U8(255)
    match x
    | let n: U8 => n == 255
    else false
    end

  fun _check_i16(): Bool =>
    let x: (String | I16) = I16(-32768)
    match x
    | let n: I16 => n == -32768
    else false
    end

  fun _check_u16(): Bool =>
    let x: (String | U16) = U16(65535)
    match x
    | let n: U16 => n == 65535
    else false
    end

  fun _check_i8_negative(): Bool =>
    let x: (String | I8) = I8(-128)
    match x
    | let n: I8 => n == -128
    else false
    end

  fun _check_f32(): Bool =>
    let x: (String | F32) = F32(3.14)
    match x
    | let n: F32 => (n > 3.13) and (n < 3.15)
    else false
    end

  fun _check_identity(): Bool =>
    let a: (String | U32) = U32(99)
    let b: (String | U32) = U32(99)
    let c: (String | U32) = U32(100)
    (a is b) and (not (a is c))

  fun _check_stringable(): Bool =>
    let x: Stringable = U32(42)
    x.string() == "42"

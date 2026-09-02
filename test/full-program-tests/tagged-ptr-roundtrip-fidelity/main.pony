use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    // U32: max value round-trip
    let u32_box: (U32 | None) = U32.max_value()
    let u32_ok = match u32_box
    | let v: U32 => v == U32.max_value()
    else false
    end

    // I32: negative value round-trip
    let i32_box: (I32 | None) = I32(-1)
    let i32_ok = match i32_box
    | let v: I32 => v == -1
    else false
    end

    // U8: boundary value round-trip
    let u8_box: (U8 | None) = U8(255)
    let u8_ok = match u8_box
    | let v: U8 => v == 255
    else false
    end

    // I8: negative boundary round-trip
    let i8_box: (I8 | None) = I8(-128)
    let i8_ok = match i8_box
    | let v: I8 => v == -128
    else false
    end

    // U16: boundary value round-trip
    let u16_box: (U16 | None) = U16(65535)
    let u16_ok = match u16_box
    | let v: U16 => v == 65535
    else false
    end

    // I16: negative boundary round-trip
    let i16_box: (I16 | None) = I16(-32768)
    let i16_ok = match i16_box
    | let v: I16 => v == -32768
    else false
    end

    // Bool: round-trip (true and false)
    let bool_box: (Bool | None) = true
    let bool_ok = match bool_box
    | let v: Bool => v == true
    else false
    end

    let bool_false_box: (Bool | None) = false
    let bool_false_ok = match bool_false_box
    | let v: Bool => v == false
    else false
    end

    // Zero-value round-trips
    let u32_zero_box: (U32 | None) = U32(0)
    let u32_zero_ok = match u32_zero_box
    | let v: U32 => v == 0
    else false
    end

    let u8_zero_box: (U8 | None) = U8(0)
    let u8_zero_ok = match u8_zero_box
    | let v: U8 => v == 0
    else false
    end

    let i32_zero_box: (I32 | None) = I32(0)
    let i32_zero_ok = match i32_zero_box
    | let v: I32 => v == 0
    else false
    end

    if u32_ok and i32_ok and u8_ok and i8_ok and u16_ok and i16_ok
      and bool_ok and bool_false_ok
      and u32_zero_ok and u8_zero_ok and i32_zero_ok
    then
      @pony_exitcode(1)
    end

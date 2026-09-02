use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    // Normal value round-trip
    let f32_box: (F32 | None) = F32(3.14)
    let normal_ok = match f32_box
    | let v: F32 => v == F32(3.14)
    else false
    end

    // Negative zero round-trip
    let negzero_box: (F32 | None) = F32.from_bits(0x80000000)
    let negzero_ok = match negzero_box
    | let v: F32 => v.bits() == 0x80000000
    else false
    end

    // Infinity round-trip
    let inf_box: (F32 | None) = F32.max_value() * F32(2)
    let inf_ok = match inf_box
    | let v: F32 => v == (F32.max_value() * F32(2))
    else false
    end

    // NaN round-trip (NaN != NaN, so compare bits)
    let nan_box: (F32 | None) = F32.from_bits(0x7FC00000)
    let nan_ok = match nan_box
    | let v: F32 => v.bits() == 0x7FC00000
    else false
    end

    if normal_ok and negzero_ok and inf_ok and nan_ok then
      @pony_exitcode(1)
    end

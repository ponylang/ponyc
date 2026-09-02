use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let a: (F32 | None) = F32(3.14)
    let b: (F32 | None) = F32(3.14)
    let same_value = a is b

    let c: (F32 | None) = F32(0)
    let d: (F32 | None) = -F32(0)
    let diff_zero = c isnt d

    let e: (F32 | None) = F32(1.0)
    let f: (F32 | None) = F32(2.0)
    let diff_value = e isnt f

    // NaN bit patterns: same bits means same tagged representation
    let g: (F32 | None) = F32.from_bits(0x7FC00000)
    let h: (F32 | None) = F32.from_bits(0x7FC00000)
    let same_nan = g is h

    if same_value and diff_zero and diff_value and same_nan then
      @pony_exitcode(1)
    end

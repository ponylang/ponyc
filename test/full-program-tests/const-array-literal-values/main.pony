actor Main
  new create(env: Env) =>
    let u8s: Array[U8] val = recover val [as U8: 0x89; 0x50; 0x4E; 0x47] end
    let f64s: Array[F64] val = recover val [as F64: 1; 2; 3] end
    let f32s: Array[F32] val = recover val [as F32: 1.5; 2.5] end
    let f64_floats: Array[F64] val = recover val [as F64: 3.14; 2.718] end
    let bools: Array[Bool] val = recover val [as Bool: true; false; true] end
    let u128s: Array[U128] val = recover val [as U128: 0; 1; 42] end
    let single: Array[U32] val = recover val [as U32: 99] end

    try
      if u8s(0)? != 0x89 then env.exitcode(1); return end
      if u8s(1)? != 0x50 then env.exitcode(1); return end
      if u8s(2)? != 0x4E then env.exitcode(1); return end
      if u8s(3)? != 0x47 then env.exitcode(1); return end
      if u8s.size() != 4 then env.exitcode(1); return end

      if f64s(0)? != 1 then env.exitcode(1); return end
      if f64s(1)? != 2 then env.exitcode(1); return end
      if f64s(2)? != 3 then env.exitcode(1); return end
      if f64s.size() != 3 then env.exitcode(1); return end

      if f32s(0)? != 1.5 then env.exitcode(1); return end
      if f32s(1)? != 2.5 then env.exitcode(1); return end
      if f32s.size() != 2 then env.exitcode(1); return end

      if f64_floats(0)? != 3.14 then env.exitcode(1); return end
      if f64_floats(1)? != 2.718 then env.exitcode(1); return end
      if f64_floats.size() != 2 then env.exitcode(1); return end

      if bools(0)? != true then env.exitcode(1); return end
      if bools(1)? != false then env.exitcode(1); return end
      if bools(2)? != true then env.exitcode(1); return end
      if bools.size() != 3 then env.exitcode(1); return end

      if u128s(0)? != 0 then env.exitcode(1); return end
      if u128s(1)? != 1 then env.exitcode(1); return end
      if u128s(2)? != 42 then env.exitcode(1); return end
      if u128s.size() != 3 then env.exitcode(1); return end

      if single(0)? != 99 then env.exitcode(1); return end
      if single.size() != 1 then env.exitcode(1); return end
    else
      env.exitcode(1)
    end

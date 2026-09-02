use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let checker = Checker(env)
    checker.check_u32(U32(12345))
    checker.check_f32(F32(2.5))
    checker.check_bool(true)
    checker.done()

actor Checker
  let _env: Env
  var _ok: Bool = true

  new create(env: Env) =>
    _env = env

  be check_u32(x: (U32 | None)) =>
    match x
    | let v: U32 => if v != 12345 then _ok = false end
    else _ok = false
    end

  be check_f32(x: (F32 | None)) =>
    match x
    | let v: F32 => if v != F32(2.5) then _ok = false end
    else _ok = false
    end

  be check_bool(x: (Bool | None)) =>
    match x
    | let v: Bool => if v != true then _ok = false end
    else _ok = false
    end

  be done() =>
    if _ok then
      @pony_exitcode(1)
    end

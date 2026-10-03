use @pony_exitcode[None](code: I32)

actor Worker[A: Any val]
  var _result: I32 = 0

  new create() =>
    None

  be run(data: A, cb: {(I32)} val) =>
    _result = 0
    cb(_result)

  be run(data: A, cb: {(I32)} val) iftype A <: Stringable val =>
    _result = 1
    cb(_result)

actor Main
  var _env: Env

  new create(env: Env) =>
    _env = env
    let w = Worker[U32]
    w.run(42, {(code: I32) => @pony_exitcode(code) })

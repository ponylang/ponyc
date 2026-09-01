use @pony_exitcode[None](code: I32)

actor Receiver
  let _main: Main

  new create(main': Main) =>
    _main = main'

  be receive(x: Any val) =>
    match x
    | let n: U32 => _main.result(n == 42)
    else _main.result(false)
    end

actor Main
  var _ok: Bool = false

  new create(env: Env) =>
    let r = Receiver(this)
    let x: Any val = U32(42)
    r.receive(x)

  be result(ok: Bool) =>
    _ok = ok
    @pony_exitcode(I32(if _ok then 1 else 0 end))

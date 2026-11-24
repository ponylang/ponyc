// An actor sends its embedded val field, which references itself. Tracing the
// message must finish, and the receiver can collect while holding it.
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])

class C
  var self_ref: (C box | None) = None
  let n: U64 = 7
  new val create() => self_ref = this

actor Keeper
  var _c: (C val | None) = None
  let _env: Env
  new create(env: Env) => _env = env
  be take(c: C val) =>
    _c = c
    @pony_triggergc(@pony_ctx())
    check()
  be check() =>
    match _c
    | let c: C val => if c.n != 7 then _env.exitcode(1) end
    end

actor Main
  embed c: C val = C

  new create(env: Env) =>
    Keeper(env).take(c)

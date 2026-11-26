// An actor embeds E, which embeds F; F references itself. The actor's
// collection must finish.
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])

class F
  var self_ref: (F | None) = None

class E
  embed f: F = F

actor Main
  embed e: E = E

  new create(env: Env) =>
    e.f.self_ref = e.f
    String(64)
    @pony_triggergc(@pony_ctx())
    check()

  be check() =>
    None

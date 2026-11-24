// An actor's embedded field references itself. The actor's own collection
// reaches the field through a pointer into the actor and must finish.
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])

class C
  var self_ref: (C | None) = None

actor Main
  embed c: C = C

  new create(env: Env) =>
    c.self_ref = c
    String(64)
    @pony_triggergc(@pony_ctx())
    check()

  be check() =>
    None

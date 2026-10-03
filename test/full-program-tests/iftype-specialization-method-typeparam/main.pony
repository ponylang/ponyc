use @pony_exitcode[None](code: I32)

class Container
  new create() => None

  fun apply[A: Any val](a: A): I32 =>
    0

  fun apply[A: Any val](a: A): I32 iftype A <: Stringable val =>
    1

actor Main
  new create(env: Env) =>
    let c = Container
    @pony_exitcode(c[U32](U32(99)))

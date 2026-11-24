use @pony_exitcode[None](code: I32)

primitive Example
  fun check[A: Any val](): I32 => 10
  fun check[B: Any val](): I32 iftype B <: U8 => 30

actor Main
  new create(env: Env) =>
    // U8 satisfies the guard: specialized body (30) must run.
    // U16 fails the guard: default body (10) must run.
    // Sum is 40.
    @pony_exitcode(Example.check[U8]() + Example.check[U16]())

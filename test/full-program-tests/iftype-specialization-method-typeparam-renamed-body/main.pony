use @pony_exitcode[None](code: I32)

primitive Example
  fun check[A: Any val](): I32 => 10
  fun check[B: Any val](): I32 iftype B <: U8 => B(42).i32()

actor Main
  new create(env: Env) =>
    // U8 satisfies the guard: specialized body uses B as U8, returns 42.
    // U16 fails the guard: default body returns 10.
    // Sum is 52.
    @pony_exitcode(Example.check[U8]() + Example.check[U16]())

primitive Bar
  fun foo[A: Any val = U8](a: A): A => a

actor Main
  new create(env: Env) =>
    let x: U8 = Bar.foo(U8(42))

primitive Bar
  fun foo[A: Any val](a: A): A => a

  fun two[A: Any val, B: Any val](a: A, b: B): A => a

actor Main
  new create(env: Env) =>
    let x: U8 = Bar.foo(U8(42))
    let y: U8 = Bar.two(U8(1), "hello")

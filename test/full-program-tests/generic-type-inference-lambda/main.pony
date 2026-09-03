primitive Bar
  fun foo[A: Any val](a: A, f: {(A): A} val): A =>
    f(a)

actor Main
  new create(env: Env) =>
    let x: U8 = Bar.foo(U8(42), {(x: U8): U8 => x + 1})

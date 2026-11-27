// f[A, B] returns (A | B). Called with U8 for both, its result type is a union
// of U8 with itself, which must be the plain U8 so the value can be assigned
// to a U8. The program must compile and print 1.
primitive P
  fun f[A: Any val, B: Any val](a: A, b: B, c: Bool): (A | B) => if c then a else b end
  fun g[B: Any val](a: U8, b: B, c: Bool): B => match P.f[U8, B](a, b, c) | let x: B => x else b end
actor Main
  new create(env: Env) =>
    let v: U8 = P.g[U8](1, 2, true)
    env.out.print(v.string())

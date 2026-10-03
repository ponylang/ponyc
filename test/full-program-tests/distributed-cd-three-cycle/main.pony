actor A
  var _b: (B | None) = None

  new create() =>
    None

  be set_b(b: B) =>
    _b = b

actor B
  var _c: (C | None) = None

  new create() =>
    None

  be set_c(c: C) =>
    _c = c

actor C
  let _a: A

  new create(a: A) =>
    _a = a

actor Main
  new create(env: Env) =>
    let a = A
    let b = B
    let c = C(a)
    a.set_b(b)
    b.set_c(c)

actor A
  var _b: (B | None) = None

  new create() =>
    None

  be set_b(b: B) =>
    _b = b

actor B
  let _a: A
  let _d: D

  new create(a: A, d: D) =>
    _a = a
    _d = d

actor D
  new create() =>
    None

actor Main
  new create(env: Env) =>
    let a = A
    let d = D
    let b = B(a, d)
    a.set_b(b)

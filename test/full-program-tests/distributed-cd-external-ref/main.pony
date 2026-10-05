actor A
  var _b: (B | None) = None

  new create() =>
    None

  be set_b(b: B) =>
    _b = b

actor B
  let _a: A

  new create(a: A) =>
    _a = a

actor W1
  var _w2: (W2 | None) = None
  let _a: A

  new create(a: A) =>
    _a = a

  be set_w2(w2: W2) =>
    _w2 = w2

actor W2
  let _w1: W1

  new create(w1: W1) =>
    _w1 = w1

actor Main
  new create(env: Env) =>
    let a = A
    let b = B(a)
    a.set_b(b)
    // W1 and W2 form their own cycle, and W1 holds a reference to A.
    // A's rc will be 2 (from B and W1), but A appears in only one
    // cycle within {A, B}. DCD must not collect {A, B} while W1
    // still references A.
    let w1 = W1(a)
    let w2 = W2(w1)
    w1.set_w2(w2)

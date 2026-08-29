class val Adder
  let _base: I64

  new val create(base: I64) =>
    _base = base

  fun val add(x: I64): I64 =>
    _base + x

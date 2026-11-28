// A struct's first embedded field shares the struct's address. Main holds the
// field through an interface, declared before the struct, so its collection
// reaches the address first by a trace that reads the field's descriptor. The
// struct's other field must still be traced.
interface I
  fun x(): U64

class C
  fun x(): U64 =>
    7

class D
  let n: U64

  new create(n': U64) =>
    n = n'

struct S
  embed c: C = C
  let d: D

  new create(d': D) =>
    d = d'

actor Main
  let _c: I
  let _s: S
  var _last: D = D(0)
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env
    _s = S(D(0xDEADBEEF))
    _c = _s.c
    churn(0)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      _last = D(i)
      k = k + 1
    end
    if _s.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if i < 200 then
      churn(i + 1)
    elseif _bad != 0 then
      _env.exitcode(1)
    end

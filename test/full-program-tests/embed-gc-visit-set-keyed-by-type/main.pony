// T starts with S, which starts with C, so all three share an address. Main
// keeps C twice and S, but not T, in that order. S's own field must survive.
class D
  let n: U64

  new create(n': U64) =>
    n = n'

class C
  var e: D = D(1)

struct S
  embed c: C = C
  var d: D = D(0xDEADBEEF)

  new create() =>
    None

struct T
  embed s: S = S
  var y: U64 = 0

  new create() =>
    None

actor Main
  let _c1: C
  let _c2: C
  let _s: S
  var _last: D = D(0)
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env
    let t = T
    _c1 = t.s.c
    _c2 = t.s.c
    _s = t.s
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

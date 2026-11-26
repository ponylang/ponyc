// S2 starts with S1, which starts with C, so all three share an address.
// Main keeps C, S1 and S2. Each one's own field must survive.
class D
  let n: U64
  new create(n': U64) => n = n'

class C
  var e: D = D(0xDEADBEEF)

struct S1
  embed c: C = C
  var d: D = D(0xDEADBEEF)
  new create() => None

struct S2
  embed s: S1 = S1
  var f: D = D(0xDEADBEEF)
  new create() => None

actor Main
  let _c: C
  let _s1: S1
  let _s2: S2
  var _last: D = D(0)
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env
    let s2 = S2
    _c = s2.s.c
    _s1 = s2.s
    _s2 = s2
    churn(0)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      _last = D(i)
      k = k + 1
    end
    if _c.e.n != 0xDEADBEEF then _bad = _bad + 1 end
    if _s1.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if _s2.f.n != 0xDEADBEEF then _bad = _bad + 1 end
    if i < 200 then churn(i + 1) elseif _bad != 0 then _env.exitcode(1) end

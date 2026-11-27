// Main keeps a struct's first two embedded fields but not the struct. The
// first field reaches the struct's address first, and the second field's
// references must still survive Main's collections. One struct has two fields
// of the same type, the other two fields of different types.
class D
  let n: U64
  new create(n': U64) => n = n'

class C
  var d: D
  new create(n: U64) => d = D(n)

class E
  var x: U64 = 1

class F
  var d: D = D(0xDEADBEEF)

struct Same
  embed a: C = C(1)
  embed b: C = C(0xDEADBEEF)
  new create() => None

struct Mixed
  embed e: E = E
  embed f: F = F
  new create() => None

actor Main
  let _a: C
  let _b: C
  let _e: E
  let _f: F
  var _last: D = D(0)
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env
    let same = Same
    _a = same.a
    _b = same.b
    let mixed = Mixed
    _e = mixed.e
    _f = mixed.f
    churn(0)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      _last = D(i)
      k = k + 1
    end
    if _b.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if _f.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if i < 200 then churn(i + 1) elseif _bad != 0 then _env.exitcode(1) end

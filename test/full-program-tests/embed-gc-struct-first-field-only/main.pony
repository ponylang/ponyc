// Main keeps only a struct's first embedded field; that field references D.
// D must survive Main's collections.
class D
  let n: U64
  new create(n': U64) => n = n'

class C
  var d: D = D(0xDEADBEEF)

struct S
  embed c: C = C
  var x: U64 = 0
  new create() => None

actor Main
  let _c: C
  var _last: D = D(0)
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env
    let s = S
    _c = s.c
    churn(0)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      _last = D(i)
      k = k + 1
    end
    if _c.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if i < 200 then churn(i + 1) elseif _bad != 0 then _env.exitcode(1) end

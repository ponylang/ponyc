// Main keeps only an embedded field from the middle of an object, not the
// object that holds it. The holder's memory must stay allocated, and the
// field's own references must still be traced.
class D
  let n: U64

  new create(n': U64) =>
    n = n'

class C
  var d: D = D(0xDEADBEEF)

class A
  var x: U64 = 0
  embed c: C = C

actor Main
  let _c: C
  var _last: A = A
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env
    let a: A ref = A
    _c = a.c
    churn(0)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      let a: A ref = A
      a.x = i
      a.c.d = D(i)
      _last = a
      k = k + 1
    end
    if _c.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if i < 200 then
      churn(i + 1)
    elseif _bad != 0 then
      _env.exitcode(1)
    end

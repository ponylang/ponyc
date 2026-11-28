// Main embeds a struct that starts with an embedded field and holds that field
// in another field. Main's collections reach the struct's address as the
// field, with no heap chunk to record it. The struct's other field must still
// be traced.
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])

class C
  var x: U64 = 7

class D
  let n: U64

  new create(n': U64) =>
    n = n'

struct S
  embed c: C = C
  var d: D = D(0xDEADBEEF)

  new create() =>
    None

actor Main
  embed s: S = S
  var _c: C
  var _last: D = D(0)
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env
    _c = s.c
    churn(0)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      _last = D(i)
      k = k + 1
    end
    @pony_triggergc(@pony_ctx())
    if s.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if i < 50 then
      churn(i + 1)
    elseif _bad != 0 then
      _env.exitcode(1)
    end

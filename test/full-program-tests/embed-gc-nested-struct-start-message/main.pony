// S2 starts with S1, which starts with C. C and S2 go in one message, C first.
// The receiver's collections must trace S2's field e.
class C
  let x: U64 = 7

class D
  let n: U64
  new create(n': U64) => n = n'

struct S1
  embed c: C = C
  var x: U64 = 0
  new create() => None

struct S2
  embed s: S1 = S1
  let e: D
  new create(e': D iso) => e = consume e'

actor Receiver
  var _c: C val
  var _s: S2 val
  let _env: Env
  var _bad: U64 = 0
  new create(env: Env) =>
    _env = env
    _s = recover S2(recover D(0xDEADBEEF) end) end
    _c = _s.s.c

  be take(c: C val, s: S2 val) =>
    _c = c
    _s = s

  be check(i: U64) =>
    if _s.e.n != 0xDEADBEEF then _bad = _bad + 1 end
    if (i == 200) and (_bad != 0) then _env.exitcode(1) end

actor Sender
  let _r: Receiver
  var _last: D = D(0)
  new create(r: Receiver) =>
    _r = r
    let s: S2 val = recover S2(recover D(0xDEADBEEF) end) end
    _r.take(s.s.c, s)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      _last = D(i)
      k = k + 1
    end
    _r.check(i)
    if i < 200 then churn(i + 1) end

actor Main
  new create(env: Env) =>
    let r = Receiver(env)
    Sender(r).churn(0)

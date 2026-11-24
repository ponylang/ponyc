// A struct's first embedded field and the struct travel in separate messages,
// field first. The receiver's collections must trace the struct's other field.
class C
  let x: U64 = 7

class D
  let n: U64
  new create(n': U64) => n = n'

struct S
  embed c: C = C
  let d: D
  new create(d': D iso) => d = consume d'

actor Receiver
  var _c: C val
  var _s: S val
  let _env: Env
  var _bad: U64 = 0
  new create(env: Env) =>
    _env = env
    _s = recover S(recover D(0xDEADBEEF) end) end
    _c = _s.c

  be take_c(c: C val) =>
    _c = c

  be take_s(s: S val) =>
    _s = s

  be check(i: U64) =>
    if _s.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if (i == 200) and (_bad != 0) then _env.exitcode(1) end

actor Sender
  let _r: Receiver
  var _last: D = D(0)
  new create(r: Receiver) =>
    _r = r
    let s: S val = recover S(recover D(0xDEADBEEF) end) end
    _r.take_c(s.c)
    _r.take_s(s)

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

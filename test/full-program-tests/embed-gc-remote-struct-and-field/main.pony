// An actor receives a struct iso and keeps both the struct and its first
// embedded field, field first. Its collections must trace the struct's other
// field, which another actor owns.
class C
  var x: U64 = 7

class D
  let n: U64
  new create(n': U64) => n = n'

struct S
  embed c: C = C
  var d: D
  new create(d': D iso) => d = consume d'

actor Receiver
  var _c: C ref
  var _s: S ref
  var _last: D = D(0)
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env
    _s = S(recover D(0) end)
    _c = _s.c

  be take(s: S iso) =>
    let s': S ref = consume s
    _s = s'
    _c = s'.c

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      _last = D(i)
      k = k + 1
    end
    if _s.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    if i < 200 then churn(i + 1) elseif _bad != 0 then _env.exitcode(1) end

actor Sender
  var _last: D = D(0)
  new create(r: Receiver) =>
    r.take(recover S(recover D(0xDEADBEEF) end) end)
    // Same sender, so take arrives before churn.
    r.churn(0)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 2000 do
      _last = D(i)
      k = k + 1
    end
    if i < 200 then churn(i + 1) end

actor Main
  new create(env: Env) =>
    let r = Receiver(env)
    Sender(r).churn(0)

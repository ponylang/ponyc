// Donor actors embed a struct that starts with an embedded field. Each sends
// the field and the struct to a keeper and finishes. Neither shares a heap
// chunk with anything, so the keeper's references must keep each donor alive
// while the keeper still holds them.
class C
  let x: U64

  new val create(x': U64) =>
    x = x'

class D
  let n: U64

  new val create(n': U64) =>
    n = n'

struct S
  embed c: C val = C(7)
  let d: D val

  new val create(d': D val) =>
    d = d'

actor Donor
  embed s: S val = S(D(0xDEADBEEF))

  new create(k: Keeper) =>
    k.take(s.c, s)

actor Filler
  embed s: S val = S(D(0x1111))

  new create() =>
    None

actor Keeper
  let _cs: Array[C val] = Array[C val]
  let _ss: Array[S val] = Array[S val]
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) =>
    _env = env

  be take(c: C val, s: S val) =>
    _cs.push(c)
    _ss.push(s)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 200 do
      Filler
      k = k + 1
    end
    for s in _ss.values() do
      if s.d.n != 0xDEADBEEF then _bad = _bad + 1 end
    end
    for c in _cs.values() do
      if c.x != 7 then _bad = _bad + 1 end
    end
    if i < 300 then
      churn(i + 1)
    elseif _bad != 0 then
      _env.exitcode(1)
    end

actor Main
  new create(env: Env) =>
    let k = Keeper(env)
    var d: USize = 0
    while d < 10 do
      Donor(k)
      d = d + 1
    end
    k.churn(0)

  fun @runtime_override_defaults(rto: RuntimeOptions) =>
    rto.ponycdinterval = 10

// Actors send their embedded val field and then have nothing left to do.
// While another actor holds the fields, the sending actors must not be
// collected: their memory would be reused by the actors created afterwards.
class C
  let n: U64
  new val create(n': U64) => n = n'

actor Donor
  embed c: C val = C(0xDEADBEEF)
  new create(k: Keeper) =>
    k.take(c)

actor Filler
  // Same layout as Donor, so a Filler can reuse a freed Donor's memory.
  embed c: C val = C(0x1111)
  new create() => None

actor Keeper
  let _cs: Array[C val] = Array[C val]
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) => _env = env

  be take(c: C val) =>
    _cs.push(c)

  be churn(i: U64) =>
    var k: USize = 0
    while k < 200 do
      Filler
      k = k + 1
    end
    for c in _cs.values() do
      if c.n != 0xDEADBEEF then _bad = _bad + 1 end
    end
    if i < 1000 then churn(i + 1) elseif _bad != 0 then _env.exitcode(1) end

actor Main
  new create(env: Env) =>
    let k = Keeper(env)
    var d: USize = 0
    while d < 10 do
      Donor(k)
      d = d + 1
    end
    k.churn(0)

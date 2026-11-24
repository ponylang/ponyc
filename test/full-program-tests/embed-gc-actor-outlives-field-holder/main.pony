// An actor sends its embedded val field and then has nothing left to do.
// While another actor holds the field, the sending actor must not be
// collected: its memory would be reused by the actors created afterwards.
class C
  let n: U64
  new val create(n': U64) => n = n'

actor Donor
  embed c: C val = C(0xDEADBEEF)
  new create(k: Keeper) =>
    k.take(c)

actor Filler
  // Same layout as Donor, so a freed Donor is reused by a Filler.
  embed c: C val = C(0x1111)
  new create() => None

actor Keeper
  var _c: (C val | None) = None
  var _bad: U64 = 0
  let _env: Env

  new create(env: Env) => _env = env

  be take(c: C val) =>
    _c = c

  be churn(i: U64) =>
    var k: USize = 0
    while k < 200 do
      Filler
      k = k + 1
    end
    match _c
    | let c: C val => if c.n != 0xDEADBEEF then _bad = _bad + 1 end
    end
    if i < 200 then churn(i + 1) elseif _bad != 0 then _env.exitcode(1) end

actor Main
  new create(env: Env) =>
    let k = Keeper(env)
    Donor(k)
    k.churn(0)

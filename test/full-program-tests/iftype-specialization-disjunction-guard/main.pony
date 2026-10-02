use @pony_exitcode[None](code: I32)

trait Hashable
  fun hash(): USize

primitive MyHash is Hashable
  fun hash(): USize => 42

class Container[A: Any val]
  var _a: A

  new create(a: A) =>
    _a = a

  fun describe(): I32 =>
    0

  fun describe(): I32 iftype A <: Stringable val or A <: Hashable val =>
    1

actor Main
  new create(env: Env) =>
    // MyHash is Hashable but not Stringable — disjunction fires.
    let c = Container[MyHash](MyHash)
    @pony_exitcode(c.describe())

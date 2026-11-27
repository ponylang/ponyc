// Q inherits f with A = (B2 | None), and mk() returns B2. string() exists on
// mk() only through the iftype on A, which need not hold for every B2, so
// compilation must fail, with a note to bind the value to a Stringable val
// local first.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end

class Q[B2: Any val] is Foo[(B2 | None)]
  let _b: B2
  new create(b: B2) => _b = b
  fun mk(): B2 => _b

actor Main
  new create(env: Env) =>
    env.out.print(Q[U8](7).f())
    env.out.print(Q[Array[U8] val]([as U8: 1]).f())

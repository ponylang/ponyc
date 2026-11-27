// G inherits f with A = (A2 | None), and G's mk() returns (A2 | None).
// string() exists on mk() only through the iftype on A, so compilation must
// fail with one error naming '(A2 | None)' and a note to bind the value to a
// Stringable val local first.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then mk().string() else "other" end
class val G[A2: Any val] is Foo[(A2 | None)]
  new val create() => None
  fun mk(): (A2 | None) => None
actor Main
  new create(env: Env) => env.out.print(G[P].f())

// G inherits f with A = (B | None), and G's mk() returns None. foo() exists
// on mk() only through the iftype on A, so compilation must fail with
// "couldn't find 'foo' in 'None'" and a note to bind the value to a HasFoo
// val local first.
interface HasFoo
  fun foo(): String
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: HasFoo val then mk().foo() else "other" end
class val G[B: Any val] is Foo[(B | None)]
  new val create() => None
  fun mk(): None => None
actor Main
  new create(env: Env) => env.out.print(G[U8].f())

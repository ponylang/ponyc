// T.f's default is a literal whose name is d. f is called only on the
// literal from T.k, which inherits f, so the call uses T's default and
// prints d.
trait T
  fun name(): String => "T"
  fun f(x: T val = object val is T fun name(): String => "d" end): String => x.name()
  fun k(): T val => object val is T end
class val C is T
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.k().f())

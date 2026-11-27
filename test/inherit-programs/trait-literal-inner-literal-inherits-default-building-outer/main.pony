// T.m returns an outer object literal whose own m returns an inner literal.
// The inner literal inherits T's m, which builds the outer literal again, so
// the program prints outer inner outer.
trait T
  fun name(): String => "T"
  fun m(): T val =>
    object val is T
      fun name(): String => "outer"
      fun m(): T val => object val is T fun name(): String => "inner" end
    end
class C is T
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.m().name() + " " + C.m().m().name() + " " + C.m().m().m().name())

// T.a and T.b each return an object literal of T with no fields. b() reached
// through a(), through a().a(), and through two b() calls must give the same
// object each time, so all three `is` checks print true. T declares b before
// a; only trait literals appear, none in ordinary code.
trait T
  fun name(): String => "T"
  fun val b(): T val => object val is T fun name(): String => "B" end
  fun val a(): T val => object val is T fun name(): String => "A" end
class val C is T
  new val create() => None
actor Main
  new create(env: Env) =>
    let x = C.a().a().b()   // $1.b()
    let y = C.a().b()       // first-level class's b (T's checked b)
    let z = C.a().a().b().b()
    env.out.print(x.name() + " " + (x is y).string() + " " + (y is z).string() + " " + (x is z).string())

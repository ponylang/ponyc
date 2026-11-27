// T.b is declared after T.a, whose literal provides T and so inherits b.
// b() called on a's literal, at any depth, must return the same type as b()
// called one level down, so the program prints B true true true.
trait T
  fun val a(): T val => object val is T fun name(): String => "A" end
  fun name(): String => "T"
  fun val b(): T val => object val is T fun name(): String => "B" end
class val C is T
  new val create() => None
actor Main
  new create(env: Env) =>
    let x = C.a().a().b()   // $1.b()
    let y = C.a().b()       // first-level class's b (T's checked b)
    let z = C.a().a().b().b()
    env.out.print(x.name() + " " + (x is y).string() + " " + (y is z).string() + " " + (x is z).string())

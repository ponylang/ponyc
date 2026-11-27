// T[A].m(a: A) returns a T[A] literal. C is T[U8] and the generic D[B] is
// T[B]; C.m(1).n() and D[String].m("x").n() must each print "n": the literal
// is typed with the A of each provider.
trait T[A: Any val]
  fun m(a: A): T[A] val => object val is T[A] end
  fun n(): String => "n"
class C is T[U8]
class D[B: Any val] is T[B]
actor Main
  new create(env: Env) => env.out.print(C.m(1).n()); env.out.print(D[String].m("x").n())

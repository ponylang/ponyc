// A struct provides trait T and inherits T's default n(), the same way a
// class does. The program must print "t".
trait T
  fun n(): String => "t"
struct S is T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(S.n())

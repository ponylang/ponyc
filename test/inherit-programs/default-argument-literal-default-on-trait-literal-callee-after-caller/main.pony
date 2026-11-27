// T.m() builds an object literal that is a T and calls h() on it; h() has an
// object literal as its default argument and is declared after m(). The
// program must compile and print "h": h()'s default can be checked while T
// is still being checked.
trait T
  fun name(): String => "t"
  fun m(): String =>
    let o = object ref is T end
    o.h()
  fun h(x: Any val = object val fun s(): String => "d" end): String => "h"
class C is T
actor Main
  new create(env: Env) => env.out.print(C.m())

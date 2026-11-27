// T.m() builds an object literal that is a T and calls h() on it; h() has an
// object literal as its default argument and is declared before m(). The
// program must compile and print "h", the same as when h() comes after m().
trait T
  fun h(x: Any val = object val fun s(): String => "d" end): String => "h"
  fun name(): String => "t"
  fun m(): String =>
    let o = object ref is T end
    o.h()
class C is T
actor Main
  new create(env: Env) => env.out.print(C.m())

// T1 declares h, whose default argument is an object literal, before m, which
// calls h through a T2 receiver. T2 inherits h. The call must use h's default
// and print h.
trait T1
  fun h(x: Any val = object val end): String => "h"
  fun m(t: T2): String => t.h()
trait T2 is T1
class C is T2
actor Main
  new create(env: Env) => env.out.print(C.m(C))

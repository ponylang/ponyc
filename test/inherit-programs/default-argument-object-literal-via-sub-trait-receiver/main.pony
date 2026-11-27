// T1.m calls h through a T2 receiver, and h's default argument is an object
// literal. T2 inherits h from T1. The call must use that default and print h.
trait T1
  fun m(t: T2): String => t.h()
  fun h(x: Any val = object val end): String => "h"
trait T2 is T1
class C is T2
actor Main
  new create(env: Env) => env.out.print(C.m(C))

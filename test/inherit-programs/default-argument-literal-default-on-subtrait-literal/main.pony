// T1.m() builds an object literal of the subtrait T2 and calls T2.h(), whose
// default argument is an object literal. The program must compile and print
// "h": checking T2 while T1 is being checked must still check h()'s default.
trait T1
  fun m(): String =>
    let o = object ref is T2 end
    o.h()
  fun z(): Any val => object val end
trait T2 is T1
  fun h(x: Any val = object val end): String => "h"
class C is T2
actor Main
  new create(env: Env) => env.out.print(C.m())

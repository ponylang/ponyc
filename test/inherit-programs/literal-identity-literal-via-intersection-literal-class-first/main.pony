// T0.fa returns a literal of (T1 & M), which inherits T1.fb. With C and D
// declared before the traits, the T2 literal fb returns must be the same
// type whether reached from C or from D, so the program prints true.
actor Main
  new create(env: Env) =>
    env.out.print((C.fa().fb() is D.fa().fb()).string())
class val C is T0
  new val create() => None
class val D is T0
  new val create() => None
trait M
trait T0
  fun val fa(): T1 val => object val is (T1 & M) end
trait T1
  fun val fb(): T2 val => object val is T2 end
trait T2

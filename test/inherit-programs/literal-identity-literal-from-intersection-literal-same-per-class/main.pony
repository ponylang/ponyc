// T0.fa returns an object literal of (T1 & M), whose inherited fb returns an
// object literal of T2. fb's literal must be the same object whether reached
// from C or from D, so `is` prints true.
trait M
trait T0
  fun val fa(): T1 val => object val is (T1 & M) end
trait T1
  fun val fb(): T2 val => object val is T2 end
trait T2
class val C is T0
  new val create() => None
class val D is T0
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print((C.fa().fb() is D.fa().fb()).string())

// T1 declares f[A] with no body and T2 gives it a body that calls A's
// constructor, directly, in a lambda and in a narrowing. C provides both, so
// its f has T1's type parameter and T2's body. The test expects C to take
// T2's body and print "narrowed x x" and "x".
primitive X
  new create() => None
  fun s(): String => "x"
trait T1
  fun f[A: X val](): String
  fun g[A: X val](): A
trait T2
  fun f[A: X val](): String =>
    let h = {(): String => A.create().s()}
    iftype A <: X val then "narrowed " + A.create().s() + " " + h() else "other" end
  fun g[A: X val](): A => A.create()
class C is (T1 & T2)
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.f[X]())
    env.out.print(C.g[X]().s())

// A lambda and an object literal in a trait method call the constructor of
// the method's type parameter A. The classes they make take A as their own
// type parameter, so the test expects each call to use X and print x three
// times.
primitive X
  new create() => None
  fun s(): String => "x"
trait T
  fun f[A: X val](): String =>
    let g = {(): String => A.create().s()}
    g()
  fun k[A: X val](): String =>
    object
      fun k(): String => A.create().s()
    end.k()
  fun n[A: X val](): String =>
    let g = {(): String => iftype A <: X val then A.create().s() else "o" end}
    g()
class C is T
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.f[X]())
    env.out.print(C.k[X]())
    env.out.print(C.n[X]())

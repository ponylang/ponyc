// T.m returns an object literal that provides T itself. Calling m() twice on
// an object literal of T made in Main, then n(), must print "n".
trait T
  fun m(): T val => object val is T end
  fun n(): String => "n"
actor Main
  new create(env: Env) =>
    let t: T val = object val is T end
    env.out.print(t.m().m().n())

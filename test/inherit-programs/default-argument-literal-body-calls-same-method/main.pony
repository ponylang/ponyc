// P.f and P.h have object literal defaults whose methods call P.g() and P.h().
// P.h() uses its own default, but only when the method runs, so it is not a
// cycle; expects dge.
trait Named
  fun name(): String
primitive P
  fun f(x: Named val = object val is Named fun name(): String => "d" + P.g() end): String => x.name()
  fun g(): String => "g"
  fun h(x: Named val = object val is Named fun name(): String => if false then P.h() else "e" end end): String => x.name()
actor Main
  new create(env: Env) => env.out.print(P.f() + P.h())

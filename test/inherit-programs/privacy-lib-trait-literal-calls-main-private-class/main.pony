// main's private class _C provides lib's Named. Named.m() captures this in a
// literal that calls me.name(). _C.m().show() must print "show _C": a call
// on the providing class is allowed even though that class is private.
use l = "lib"
class val _C is l.Named
  new val create() => None
  fun name(): String => "_C"
actor Main
  new create(env: Env) => env.out.print(_C.m().show())

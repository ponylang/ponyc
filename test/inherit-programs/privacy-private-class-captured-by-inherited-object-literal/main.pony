// main's private class _C inherits lib's Named.m, whose object literal
// captures this as _C and calls name() on it. The literal must compile even
// though lib cannot name _C, and the program must print "show _C".
use l = "lib"
class val _C is l.Named
  new val create() => None
  fun name(): String => "_C"
actor Main
  new create(env: Env) => env.out.print(_C.m().string())

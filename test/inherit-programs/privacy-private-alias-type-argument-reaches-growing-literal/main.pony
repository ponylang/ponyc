// C in main is lib's T[PubH], a public alias of lib's private _H. T.m()
// returns a growing T[Array[A] val] literal that never uses A. C.m().m().n()
// must print "n": passing the alias along through literals is allowed.
use l = "lib"
class C is l.T[l.PubH]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().m().n())

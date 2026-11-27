// main's private primitive _Impl provides lib's T. T.f, written in lib, puts
// this in a local, an array and a tuple, and calls a lambda. In _Impl's copy
// these all have main's private type. The copy must still compile, and the
// program must print "i2ii!".
use l = "lib"
primitive _Impl is l.T
  fun g(): String => "i"
actor Main
  new create(env: Env) => env.out.print(_Impl.f())

// main uses lib's private _H, through lib's public alias PubH, only as the
// type argument of lib's Box. lib's Foo has a growing literal that captures
// a value of its type parameter. The program must print "lib._H": Foo's
// literal must type check on its own.
use l = "lib"
actor Main
  new create(env: Env) => env.out.print(l.Box[l.PubH](l.MakeH()).show())

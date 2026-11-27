// lib's a.pony calls Pub.f() using its default argument, which calls lib's
// private _H; both are declared in b.pony, after a.pony. main calls it both
// through First.go and directly. The program must print "77".
use l = "lib"
actor Main
  new create(env: Env) => env.out.print(l.First.go().string() + l.Pub.f().string())

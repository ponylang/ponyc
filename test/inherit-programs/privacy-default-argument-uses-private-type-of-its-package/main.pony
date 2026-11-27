// lib's Pub.f has a default argument that calls lib's private primitive _H.
// main calls l.Pub.f() and uses that default. The default is judged in lib,
// where it is written and _H is visible, so the program must print 7.
use l = "lib"
actor Main
  new create(env: Env) => env.out.print(l.Pub.f().string())

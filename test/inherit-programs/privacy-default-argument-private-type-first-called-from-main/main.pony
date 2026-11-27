// lib3's Pub.f has a default argument that calls lib3's private primitive _H,
// and the only caller is main. The default is judged in lib3, where it is
// written and _H is visible, so the program must compile and print 7.
use l = "lib3"
actor Main
  new create(env: Env) => env.out.print(l.Pub.f().string())

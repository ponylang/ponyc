// lib's T.m has a type parameter named B and returns an object literal that
// uses b; main has no type named B. C inherits m and C.m[U8](7) must print 7.
use l = "lib"
primitive Q
  fun string(): String iso^ => "B".clone()
class C is l.T
actor Main
  new create(env: Env) =>
    env.out.print(C.m[U8](7).string())

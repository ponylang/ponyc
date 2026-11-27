// lib's T.m has a type parameter named B and returns an object literal that
// uses b. main declares a primitive named B and C inherits m in main. B in the
// copy must still mean m's type parameter, so C.m[U8](7) prints 7.
use l = "lib"
primitive B
  fun string(): String iso^ => "B".clone()
class C is l.T
actor Main
  new create(env: Env) =>
    env.out.print(C.m[U8](7).string())

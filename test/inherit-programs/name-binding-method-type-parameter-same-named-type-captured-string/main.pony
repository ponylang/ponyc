// lib's T.m has a type parameter B, builds a String from b, and returns an
// object literal that captures the String. main declares a primitive named
// B; B in C's inherited copy must still mean m's type parameter, printing 7.
use l = "lib"
primitive B
  fun string(): String iso^ => "B".clone()
class C is l.T
actor Main
  new create(env: Env) =>
    env.out.print(C.m[U8](7).string())

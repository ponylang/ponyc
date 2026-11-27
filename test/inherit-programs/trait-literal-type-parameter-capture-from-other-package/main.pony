// lib's T.m returns a literal that captures x: A, and C in main.pony
// provides lib's T[U8]. The literal and the literal made from it must
// return their own captured values, printing 3 4.
use l = "lib"
class val C is l.T[U8]
  new val create() => None
  fun get(): U8 => 1
actor Main
  new create(env: Env) =>
    let a = C.m(3)
    let b = a.m(4)
    env.out.print(a.get().string() + " " + b.get().string())

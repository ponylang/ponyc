// T has two iftype specializations of d(), and with A = Array[U8] and B = U8
// the first guard covers every type the second does. Expects the program to
// compile and print "arrB": the first matching guard is used, and the shadowed
// one in inherited code is not reported.
trait T[A: Any val, B: Any val]
  fun d(a: A): String => "default"
  fun d(a: A): String iftype A <: Array[B] val => "arrB"
  fun d(a: A): String iftype A <: Array[U8] val => "arrU8"
class C is T[Array[U8] val, U8]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.d([as U8: 1]))

// T.m returns an object literal that provides T itself, and no type provides
// T. The program must compile and print "ok".
trait T
  fun m(): T val => object val is T end
  fun n(): String => "n"
actor Main
  new create(env: Env) => env.out.print("ok")

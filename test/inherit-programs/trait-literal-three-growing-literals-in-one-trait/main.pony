// T[A] has three methods, each returning a literal of T[W1[A]], T[W2[A]],
// or T[W3[A]], so the literals refer to each other's types without end.
// Nothing calls them; the program must compile and print "x".
class val W1[X: Any val]
class val W2[X: Any val]
class val W3[X: Any val]
trait T[A: Any val]
  fun n(): String => "n"
  fun m1(): T[W1[A]] val => object val is T[W1[A]] end
  fun m2(): T[W2[A]] val => object val is T[W2[A]] end
  fun m3(): T[W3[A]] val => object val is T[W3[A]] end
actor Main
  new create(env: Env) => env.out.print("x")

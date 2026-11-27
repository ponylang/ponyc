// m captures s: String = name() and the literal's name returns "in " + s.
// C's literal captures C's name and the next literal captures the first
// literal's, so the program prints in C|in in C.
trait T
  fun name(): String => "T"
  fun m(): T val =>
    let s: String = name()
    object val is T fun name(): String => "in " + s end
class C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) =>
    env.out.print(C.m().name() + "|" + C.m().m().name())

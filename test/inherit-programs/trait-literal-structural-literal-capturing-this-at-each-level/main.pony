// T.w captures this and returns a literal that provides no trait. C.w()
// names C, and w on m's literal names that literal, printing "w of C" then
// "w of anon".
interface val N
  fun name(): String
trait T
  fun name(): String => "T"
  fun m(): T val => object val is T fun name(): String => "anon" end
  fun val w(): N =>
    let me = this
    object val fun name(): String => "w of " + me.name() end
primitive C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) =>
    env.out.print(C.w().name())
    env.out.print(C.m().w().name())

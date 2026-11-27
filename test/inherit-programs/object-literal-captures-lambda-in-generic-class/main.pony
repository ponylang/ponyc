// K[A].m builds a lambda that uses Array[A], and an object literal that
// captures the lambda. K[U8].m().n() must call the lambda and print 1.
interface N
  fun n(): String
class val K[A: Any val]
  new val create() => None
  fun m(): N val =>
    let f = {(): USize => Array[A].size() + 1}
    object val is N
      fun n(): String => f().string()
    end
actor Main
  new create(env: Env) => env.out.print(K[U8].m().n())

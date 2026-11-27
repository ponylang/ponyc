// T.d recurses through m(), whose literal provides T[Array[A] val], so each
// level has a larger type argument. Nothing provides T; the program must
// still compile and print nothing.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
  fun d(k: USize): USize => if k == 0 then 0 else m().d(k - 1) + 1 end
actor Main
  new create(env: Env) => None

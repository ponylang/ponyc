// T's method builds an object literal providing (T2 & M), and T2's default has
// a type error. T has a second type error after it. The test expects only the
// error in T2, the same as when the literal provides T2 alone.
trait T
  fun m(): T2 => object is (T2 & M) end
  fun bad(): U8 => "t-after"
trait M
trait T2
  fun f(): U8 => "t2"
actor Main
  new create(env: Env) => None

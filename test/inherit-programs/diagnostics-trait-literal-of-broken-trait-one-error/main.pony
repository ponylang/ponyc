// T's method builds an object literal providing T2, whose default has a type
// error, and T has a second type error after it. The test expects only the
// error in T2: an error inside an object literal's members ends the pass.
trait T
  fun m(): T2 => object is T2 end
  fun bad(): U8 => "t-after"
trait T2
  fun f(): U8 => "t2"
actor Main
  new create(env: Env) => None

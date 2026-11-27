// As in the program without a lambda, but Impl's body uses its parameter b
// inside a lambda. The test expects the lambda to capture the class's own b
// and f(1, 41) to return 42.
trait Decl
  fun f(a: U8, b: U8): U8
trait Impl
  fun f(a: U8, b: U8): U8 => {(): U8 => b + 1}()
trait Mk
  fun apply(): Impl val =>
    object val is (Decl & Impl) end
primitive Make is Mk
actor Main
  new create(env: Env) =>
    env.out.print(Make().f(1, 41).string())

// The object literal in Mk provides Decl, which declares f with no body, and
// Impl, which gives f a body using its parameter b. The literal's class takes
// Impl's body under Decl's signature, so b must be the class's own parameter.
// The test expects f(1, 41) to return 42.
trait Decl
  fun f(a: U8, b: U8): U8
trait Impl
  fun f(a: U8, b: U8): U8 => b + 1
trait Mk
  fun apply(): Impl val =>
    object val is (Decl & Impl) end
primitive Make is Mk
actor Main
  new create(env: Env) =>
    env.out.print(Make().f(1, 41).string())

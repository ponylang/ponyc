// An object literal in Main provides T2, which provides T3, and T3's default
// bad() has a type error that T2 overrides. Main has a second type error after
// the literal. The test expects both errors: checking the literal must not
// stop the compiler before Main's own error is reported.
actor Main
  new create(env: Env) =>
    let o = object is T2 end
    let x: U8 = "main"
trait T2 is T3
  fun bad(): U8 => 2
trait T3
  fun bad(): U8 => "t3"

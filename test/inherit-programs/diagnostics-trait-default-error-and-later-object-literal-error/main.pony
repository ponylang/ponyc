// Trait T's default f() has a type error. Main builds an object literal
// providing T and then has a second type error. The test expects both errors:
// the literal must not repeat T's error or hide Main's.
trait T
  fun f(): U8 =>
    let x: U8 = "s"
    x
  fun g(): U8 => 1
actor Main
  new create(env: Env) =>
    let o = object val is T end
    let y: U8 = "t"
    env.out.print(o.g().string())

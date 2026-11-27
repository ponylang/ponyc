// T's make returns an object literal that captures the local x, and a lambda
// inside the literal declares its own parameter x. The literal still
// captures the outer x, so C's make must build it with that value. The test
// expects twice() to double 21, printing 42.
interface Getter
  fun get(): U8
  fun twice(): U8
trait T
  fun n(): U8
  fun make(): Getter val =>
    let x: U8 = n()
    object val is Getter
      fun get(): U8 => x
      fun twice(): U8 => {(x: U8): U8 => x * 2}(get())
    end
class C is T
  fun n(): U8 => 21
actor Main
  new create(env: Env) => env.out.print(C.make().twice().string())

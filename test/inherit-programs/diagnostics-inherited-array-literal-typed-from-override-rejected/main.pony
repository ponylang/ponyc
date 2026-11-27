// C's mk() returns only U8, so in C's copy of f() the array [mk()] holds U8
// and a.push(None) is wrong. Expects "argument not assignable to parameter"
// with a note naming method 'f' inherited by C from Foo, so the error points
// at the class whose copy fails.
trait Foo
  fun mk(): (U8 | None)
  fun f(): USize =>
    let a = [mk()]
    a.push(None)
    a.size()
class C is Foo
  new create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f().string())

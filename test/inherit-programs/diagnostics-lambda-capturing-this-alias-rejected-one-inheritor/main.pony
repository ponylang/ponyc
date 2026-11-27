// A trait default binds `me = this` and captures `me` in a lambda, which the
// compiler rejects with "Foo box is not a subtype of Foo ref^". One class
// provides the trait. The test expects that error once, at the trait's text.
trait Foo
  fun g(): U8 ?
  fun f(): U8 =>
    let me = this
    let h = {()(me): U8 ? => me.g()?}
    try h()? else 0 end
class C is Foo
  new create() => None
  fun g(): U8 => 7
actor Main
  new create(env: Env) => env.out.print(C.f().string())

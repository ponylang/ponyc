// A trait default captures `s = this` in a lambda, which the compiler rejects
// with "Foo box is not a subtype of Foo ref^". Class C provides the trait. The
// test expects that one error from the trait's text.
trait Foo
  fun g(): U8 ?
  fun f(): U8 =>
    let h = {()(s = this): U8 ? => s.g()?}
    try h()? else 0 end
class C is Foo
  new create() => None
  fun g(): U8 => 7
actor Main
  new create(env: Env) => env.out.print(C.f().string())

// A trait default binds `me = this` and captures `me` in a lambda, which the
// compiler rejects with "Foo box is not a subtype of Foo ref^". No type
// provides the trait. The test expects that one error from the trait's text.
trait Foo
  fun g(): U8 ?
  fun f(): U8 =>
    let me = this
    let h = {()(me): U8 ? => me.g()?}
    try h()? else 0 end
actor Main
  new create(env: Env) => None

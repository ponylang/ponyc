primitive _L
  fun x(): String => "lib._L"
trait T
  fun foo(): _L => _L
  fun bar(): String =>
    let me = this
    let f = {()(me): String => me.foo().x()}
    f()

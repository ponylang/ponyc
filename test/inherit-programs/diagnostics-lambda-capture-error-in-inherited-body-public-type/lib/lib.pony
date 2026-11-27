primitive L
  fun x(): String => "lib.L"
trait T
  fun foo(): L => L
  fun bar(): String =>
    let me = this
    let f = {()(me): String => me.foo().x()}
    f()

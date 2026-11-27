class _Helper
  new create() => None
  fun name(): String => "lib._Helper"
trait Foo
  fun make(): String =>
    let f = _Helper~create()
    f().name()

class Helper
  new create() => None
  fun name(): String => "lib.Helper"
trait Foo
  fun make(): String =>
    let f = Helper~create()
    f().name()

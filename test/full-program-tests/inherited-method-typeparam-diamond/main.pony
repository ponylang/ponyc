// Classes inherit the same generic default method through two paths: two
// child traits, or the root trait directly and a child trait. The copies must
// be recognised as the same method, and each class's own type parameter
// shares the method type parameter's name.
use @pony_exitcode[None](code: I32)

trait Root
  fun tag wrap[T: Stringable val](t: T): String => "<" + t.string() + ">"

trait Left is Root
trait Right is Root

class Both[T: Any val] is (Left & Right)

class RootAndLeft[T: Any val] is (Root & Left)

actor Main
  new create(env: Env) =>
    if Both[None].wrap[U8](3) != "<3>" then
      @pony_exitcode(1)
    end
    if RootAndLeft[None].wrap[U8](4) != "<4>" then
      @pony_exitcode(2)
    end

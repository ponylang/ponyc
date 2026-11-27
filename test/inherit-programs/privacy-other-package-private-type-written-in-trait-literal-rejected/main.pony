// A trait method in main returns an object literal whose method declares a
// local of lib's private type _Hidden. main can't name another package's
// private type, inside a literal or not, so the test expects the private-type
// error where the literal's text writes l._Hidden.
use l = "lib"
trait val Foo
  fun mk(): Stringable val =>
    object val is Stringable
      fun string(): String iso^ =>
        let h: (l._Hidden | None) = None
        "x".clone()
    end
class val C is Foo
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.mk().string())

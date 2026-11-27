// Bar fills A with U8, which is Stringable but not a ReadSeq[U8], so the
// outer iftype branch runs and the inner one, which calls x.size(), can't.
// Expects "s": U8 having no size() is not an error in the inner branch.
trait Foo[A: Any val]
  fun f(x: A): String =>
    iftype A <: Stringable val then
      iftype A <: ReadSeq[U8] val then x.string() + x.size().string() else "s" end
    else "o" end
class Bar is Foo[U8]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(3))

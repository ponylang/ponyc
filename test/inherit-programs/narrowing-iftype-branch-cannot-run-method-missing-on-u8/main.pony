// Baz fills In with U8, so describe()'s iftype In <: String branch, which
// calls x.size(), can't run in Baz's copy. Expects "other": U8 having no
// size() is not an error in that branch.
trait Foo[In: Any val]
  fun describe(x: In): String =>
    iftype In <: String then x.size().string() else "other" end
class Baz is Foo[U8]
actor Main
  new create(env: Env) => env.out.print(Baz.describe(3))

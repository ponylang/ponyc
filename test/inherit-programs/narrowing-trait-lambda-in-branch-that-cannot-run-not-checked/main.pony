// Foo.describe has a lambda calling x.size() only in the branch where In is
// String. B provides Foo[U8], so that branch never runs for B and its lambda
// is not checked against U8. Expects "other".
trait Foo[In: Any val]
  fun describe(x: In): {(): String} val =>
    iftype In <: String then {()(x): String => x.size().string()} val else {(): String => "other"} val end
class B is Foo[U8]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(B.describe(3)())

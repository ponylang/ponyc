// C inherits a _final() whose iftype branch calls mk().string(), and C fills
// A with P, which is not Stringable, so the branch can't run. Expects the
// program to compile and print "ok": the finaliser's dead branch is no error.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun _final() =>
    iftype A <: Stringable val then
      let s: String = mk().string()
    end
class C is Foo[P]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) =>
    C
    env.out.print("ok")

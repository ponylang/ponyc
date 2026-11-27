// Foo's default stores name() in a `String val` local and passes it twice.
// Class O overrides name() to return String iso^. The test expects 4: the
// inherited body keeps the local's declared val type.
primitive Sink
  fun apply(s: String val): USize => s.size()
trait Foo
  fun name(): String val
  fun f(): USize =>
    let s: String val = name()
    Sink(s) + Sink(s)
actor Main
  new create(env: Env) =>
    let o = O
    env.out.print(o.f().string())
class O is Foo
  new create() => None
  fun name(): String iso^ => "ab".clone()

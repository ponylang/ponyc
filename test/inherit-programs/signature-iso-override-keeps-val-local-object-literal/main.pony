// Foo's default stores name() in a `String val` local and passes it twice. An
// object literal overrides name() to return String iso^. The test expects 4:
// the inherited body keeps the local's declared val type.
primitive Sink
  fun apply(s: String val): USize => s.size()
trait Foo
  fun name(): String val
  fun f(): USize =>
    let s: String val = name()
    Sink(s) + Sink(s)
actor Main
  new create(env: Env) =>
    let o = object is Foo fun name(): String iso^ => "ab".clone() end
    env.out.print(o.f().string())

// An object literal's name() returns String iso^, so in its copy of Foo.f the
// local s is iso and can't be passed to Sink twice. Expects an error for each
// argument, with the note naming the object literal's inherited f.
primitive Sink
  fun apply(s: String val): USize => s.size()
trait Foo
  fun name(): String val
  fun f(): USize =>
    let s = name()
    Sink(s) + Sink(s)
actor Main
  new create(env: Env) =>
    let o = object is Foo fun name(): String iso^ => "ab".clone() end
    env.out.print(o.f().string())

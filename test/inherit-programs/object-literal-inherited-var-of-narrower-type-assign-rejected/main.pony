// An object literal's mk returns U8, so in its copy of Foo.f the var v is U8
// and assigning None to it is an error. Expects that error with the note
// naming the object literal's inherited f.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    var v = mk()
    v = None
    "ok"
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f())

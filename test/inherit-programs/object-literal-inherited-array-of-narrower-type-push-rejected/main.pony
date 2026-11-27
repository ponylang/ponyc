// An object literal's mk returns U8, so in its copy of Foo.f the array [mk()]
// holds U8 and pushing None is an error. Expects that error with the note
// naming the object literal's inherited f.
trait Foo
  fun mk(): (U8 | None)
  fun f(): USize =>
    let a = [mk()]
    a.push(None)
    a.size()
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f().string())

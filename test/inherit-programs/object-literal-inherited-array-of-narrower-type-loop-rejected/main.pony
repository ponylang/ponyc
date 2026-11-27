// An object literal's mk returns U8, so in its copy of Foo.f the array [mk()]
// holds U8 and pushing None is an error, though f then loops over the array.
// Expects that error with the note naming the object literal's inherited f.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    let a = [mk()]
    a.push(None)
    var s = String
    for e in a.values() do
      s.append(match e | let u: U8 => u.string() | None => "none" end)
      s.append(",")
    end
    s.clone()
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f())

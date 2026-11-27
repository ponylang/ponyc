// An object literal's mk returns U8, so in its copy of Foo.f the var v is U8
// and assigning None to it between two matches is an error. Expects that
// error with the note naming the object literal's inherited f.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    var v = mk()
    let s1 = match v | let u: U8 => u.string() | None => "none" end
    v = None
    let s2 = match v | let u: U8 => u.string() | None => "none" end
    s1 + "/" + s2
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f())

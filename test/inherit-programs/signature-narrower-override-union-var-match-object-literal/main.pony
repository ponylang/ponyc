// Foo's default matches a (U8 | None) var holding mk(), assigns None, and
// matches again. An object literal overrides mk() to return U8. The test
// expects "3/none": both matches see the var's declared union type.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    var v: (U8 | None) = mk()
    let s1 = match v | let u: U8 => u.string() | None => "none" end
    v = None
    let s2 = match v | let u: U8 => u.string() | None => "none" end
    s1 + "/" + s2
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f())

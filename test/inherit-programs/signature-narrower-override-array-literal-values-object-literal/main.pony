// Foo's default builds an Array[(U8 | None)] from mk(), pushes None, and
// prints each element. An object literal overrides mk() to return U8. The test
// expects "3,none,", so both elements keep their values.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    let a: Array[(U8 | None)] = [mk()]
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

// Foo's default builds an Array[(U8 | None)] from mk() and pushes None. An
// object literal overrides mk() to return U8. The test expects 2: the
// inherited body keeps the array's declared element type.
trait Foo
  fun mk(): (U8 | None)
  fun f(): USize =>
    let a: Array[(U8 | None)] = [mk()]
    a.push(None)
    a.size()
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f().string())

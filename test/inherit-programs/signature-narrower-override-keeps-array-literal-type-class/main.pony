// Foo's default builds an Array[(U8 | None)] from mk() and pushes None. C
// overrides mk() to return U8. The test expects 2: the inherited body keeps
// the array's declared element type.
trait Foo
  fun mk(): (U8 | None)
  fun f(): USize =>
    let a: Array[(U8 | None)] = [mk()]
    a.push(None)
    a.size()
class C is Foo
  new create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f().string())

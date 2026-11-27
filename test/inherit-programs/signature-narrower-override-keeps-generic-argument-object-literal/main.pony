// Foo's default builds Cell[(U8 | None)] from mk() and stores None in it. An
// object literal overrides mk() to return U8. The test expects "ok": the
// inherited body keeps the explicit type argument.
class Cell[A: Any val]
  var v: A
  new create(v': A) => v = v'
  fun ref set(v': A) => v = v'
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    let c = Cell[(U8 | None)](mk())
    c.set(None)
    "ok"
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f())

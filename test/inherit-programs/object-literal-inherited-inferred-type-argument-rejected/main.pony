// An object literal's mk returns U8, so in its copy of Foo.f, Cell(mk()) is a
// Cell[U8] and setting it to None is an error. Expects that error with the
// note naming the object literal's inherited f.
class Cell[A: Any val]
  var v: A
  new create(v': A) => v = v'
  fun ref set(v': A) => v = v'
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    let c = Cell(mk())
    c.set(None)
    "ok"
actor Main
  new create(env: Env) =>
    let o = object is Foo fun mk(): U8 => 3 end
    env.out.print(o.f())

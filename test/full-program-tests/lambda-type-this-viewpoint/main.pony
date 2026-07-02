use @pony_exitcode[None](code: I32)

class Container[A: Stringable #read]
  var _value: A

  new create(value: A) =>
    _value = value

  fun box apply(f: {(this->A)}) =>
    f(_value)

actor Main
  new create(env: Env) =>
    // Call on a ref receiver: this->A adapts through ref.
    // ref->String ref = String ref, so ref <: box and the lambda compiles.
    let c: Container[String ref] ref = Container[String ref]("hello".clone())
    c({(s: String box) =>
      if s == "hello" then
        @pony_exitcode(1)
      end
    })

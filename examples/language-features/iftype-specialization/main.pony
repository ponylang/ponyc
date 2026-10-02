class val MyVal
  """
  A value type that does not implement `Stringable`.
  """
  let x: U32

  new val create(x': U32) =>
    x = x'

class Container[A: Any val]
  """
  A generic container whose `describe` method uses a specialized body when `A`
  is `Stringable`.
  """
  var _data: A

  new create(data: A) =>
    _data = data

  fun describe(): String =>
    "opaque value"

  fun describe(): String iftype A <: Stringable val =>
    _data.string()

actor Main
  new create(env: Env) =>
    // MyVal does not implement Stringable — default is selected.
    let c1 = Container[MyVal val](MyVal(42))
    env.out.print(c1.describe())  // prints "opaque value"

    // U32 implements Stringable — specialization is selected.
    let c2 = Container[U32](99)
    env.out.print(c2.describe())  // prints "99"

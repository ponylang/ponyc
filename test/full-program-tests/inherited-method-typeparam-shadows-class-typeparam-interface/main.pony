// `Mapped[T, R]` provides the interface `Events[R]`, whose default method
// `map` declares its own `R`. The class's `R` and the method's `R` must stay
// distinct in the inherited copy of `map`.
use @pony_exitcode[None](code: I32)

interface Events[T: Any val]
  fun ref get_event(): T

  fun ref map[R: Any val](f: {(T): R} val): Events[R] =>
    Mapped[T, R](this, f)

class Mapped[T: Any val, R: Any val] is Events[R]
  let _from: Events[T]
  let _f: {(T): R} val

  new create(from: Events[T], f: {(T): R} val) =>
    _from = from
    _f = f

  fun ref get_event(): R =>
    _f(_from.get_event())

class Source is Events[USize]
  fun ref get_event(): USize => 5

actor Main
  new create(env: Env) =>
    let strings = Source.map[String]({(n: USize): String => n.string()})
    let sizes = strings.map[USize]({(s: String): USize => s.size() + 10})
    if sizes.get_event() != 11 then
      @pony_exitcode(1)
    end

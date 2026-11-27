// Mapped's map names its own type Mapped[R, R2], with map's own R2 as a type
// argument. Checking that Mapped[R, R2] is an Events[R2] reifies map for that
// receiver, and map's own R2 must not be confused with the argument R2.
// The program must compile and print 11.
interface Events[T: Any val]
  fun ref get_event(): T
  fun ref map[R2: Any val](f: {(T): R2} val): Events[R2]

class Mapped[T: Any val, R: Any val] is Events[R]
  let _from: Events[T]
  let _f: {(T): R} val

  new create(from: Events[T], f: {(T): R} val) =>
    _from = from
    _f = f

  fun ref get_event(): R => _f(_from.get_event())

  fun ref map[R2: Any val](f: {(R): R2} val): Events[R2] =>
    Mapped[R, R2](this, f)

class Source is Events[USize]
  fun ref get_event(): USize => 5
  fun ref map[R2: Any val](f: {(USize): R2} val): Events[R2] =>
    Mapped[USize, R2](this, f)

actor Main
  new create(env: Env) =>
    let s = Source.map[String]({(n: USize): String => n.string()})
    env.out.print(s.map[USize]({(x: String): USize => x.size() + 10}).get_event().string())

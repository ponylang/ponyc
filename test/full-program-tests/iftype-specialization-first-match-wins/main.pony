use @pony_exitcode[None](code: I32)

trait val Marker

primitive Tagged is (Stringable & Marker)
  fun string(): String iso^ =>
    "tagged".clone()

class Container[A: Any val]
  var _data: A

  new create(data: A) =>
    _data = data

  fun get(): I32 =>
    0

  fun get(): I32 iftype A <: Stringable val =>
    1

  fun get(): I32 iftype A <: Marker val =>
    2

actor Main
  new create(env: Env) =>
    // Tagged satisfies both Stringable and Marker;
    // the Stringable guard appears first and should win.
    let c = Container[Tagged](Tagged)
    @pony_exitcode(c.get())

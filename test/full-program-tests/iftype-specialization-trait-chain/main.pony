use @pony_exitcode[None](code: I32)

trait Base[A: Any val]
  fun describe(): I32 => 0
  fun describe(): I32 iftype A <: Stringable val => 1

trait Middle[A: Any val] is Base[A]

class Impl[A: Any val] is Middle[A]
  var _a: A
  new create(a: A) => _a = a

actor Main
  new create(env: Env) =>
    let x = Impl[U32](42)
    @pony_exitcode(x.describe())

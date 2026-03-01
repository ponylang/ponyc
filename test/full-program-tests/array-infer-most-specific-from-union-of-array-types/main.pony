use @pony_exitcode[None](code: I32)

trait iso T1
trait iso T2
trait iso T3 is T1
trait iso T4 is T2
class iso C1 is T3
class iso C2 is T4

primitive Foo
  fun apply(): (Array[T1] | Array[T2] | Array[T3] | Array[T4]) =>
    [C1]

actor Main
  new create(env: Env) =>
    match \exhaustive\ Foo()
    | let a: Array[T1] => @pony_exitcode(1)
    | let a: Array[T2] => @pony_exitcode(2)
    | let a: Array[T3] => @pony_exitcode(3)
    | let a: Array[T4] => @pony_exitcode(4)
    end

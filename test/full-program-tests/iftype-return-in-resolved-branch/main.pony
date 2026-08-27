use "collections"
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    Foo[Array[String], String](["111"])
    @pony_exitcode(1)

primitive Foo[A: Seq[B] ref, B: Comparable[B] #read]
  fun apply(a: A) =>
    iftype A <: Array[B] then
      return None
    end
    None

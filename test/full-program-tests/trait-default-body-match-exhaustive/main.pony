use @pony_exitcode[None](code: I32)

class val A
  let n: I32
  new val create(n': I32) => n = n'

class val B
  let n: I32
  new val create(n': I32) => n = n'

trait T
  fun get(): (A | B)

  fun check(): I32 =>
    match \exhaustive\ get()
    | let a: A => a.n
    | let b: B => b.n
    end

class C is T
  new create() => None
  fun get(): (A | B) => A(42)

actor Main
  new create(env: Env) =>
    @pony_exitcode(C.check())

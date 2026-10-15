use @pony_exitcode[None](code: I32)

class val A
  let n: I32
  new val create(n': I32) => n = n'

class val B
  let n: I32
  new val create(n': I32) => n = n'

trait T
  fun get(): (A | B | None)

  fun check(): I32 =>
    var result: I32 = 0
    match get()
    | let a: A => result = a.n
    | let b: B => result = b.n
    end
    result

class C is T
  new create() => None
  fun get(): (A | B | None) => B(42)

actor Main
  new create(env: Env) =>
    @pony_exitcode(C.check())

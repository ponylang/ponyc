use @pony_exitcode[None](code: I32)

class val R
  let n: I32
  new val create(n': I32) => n = n'

trait T
  fun get(): (R | None)

  fun check(): I32 =>
    var took_arm: I32 = 0
    match get()
    | let r: R => took_arm = 1
    end
    took_arm

class C is T
  new create() => None
  fun get(): (R | None) => R(1)

actor Main
  new create(env: Env) =>
    @pony_exitcode(C.check())

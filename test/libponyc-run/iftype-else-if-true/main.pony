use @pony_exitcode[None](code: I32)

trait T
class C1 is T
class C2 is T

actor Main
  new create(env: Env) =>
    foo[C2](C2)

  fun foo[A: T](x: A) =>
    iftype A <: C1 then
      None
    elseif A <: C2 then
      @pony_exitcode(I32(1))
    end

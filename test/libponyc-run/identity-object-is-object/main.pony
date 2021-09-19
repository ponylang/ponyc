use @pony_exitcode[None](code: I32)

class C1
class C2

actor Main
  new create(env: Env) =>
    let c1: Any = C1
    let c2: Any = C2
    if (c1 is c1) and (c1 isnt c2) then
      @pony_exitcode(I32(1))
    end

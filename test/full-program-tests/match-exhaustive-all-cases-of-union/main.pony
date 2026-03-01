use @pony_exitcode[None](code: I32)

class C1 fun one(): I32 => 1
class C2 fun two(): I32 => 2
class C3 fun three(): I32 => 3

primitive Foo
  fun apply(c': (C1 | C2 | C3)): I32 =>
    match \exhaustive\ c'
    | let c: C1 => c.one()
    | let c: C2 => c.two()
    | let c: C3 => c.three()
    end

actor Main
  new create(env: Env) =>
    @pony_exitcode(Foo(C3))

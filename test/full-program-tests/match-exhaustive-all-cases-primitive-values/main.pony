use @pony_exitcode[None](code: I32)

primitive P1 fun one(): I32 => 1
primitive P2 fun two(): I32 => 2
primitive P3 fun three(): I32 => 3

primitive Foo
  fun apply(p': (P1 | P2 | P3)): I32 =>
    match \exhaustive\ p'
    | let p: P1 => p.one()
    | let p: P2 => p.two()
    | let p: P3 => p.three()
    end

actor Main
  new create(env: Env) =>
    @pony_exitcode(Foo(P3))

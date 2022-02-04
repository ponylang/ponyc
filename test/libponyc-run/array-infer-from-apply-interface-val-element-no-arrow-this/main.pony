trait val T
primitive P1 is T
primitive P2 is T
primitive P3 is T

interface ArrayishOfT
  fun apply(index: USize): T ?

primitive Foo
  fun apply(): ArrayishOfT =>
    [P1; P2; P3]

actor Main
  new create(env: Env) =>
    None

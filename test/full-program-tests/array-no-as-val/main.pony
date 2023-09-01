trait val T
primitive P1 is T
primitive P2 is T
primitive P3 is T

primitive Foo
  fun apply(): Array[(P1 | P2 | P3)] =>
    [P1; P2; P3]

actor Main
  new create(env: Env) =>
    None

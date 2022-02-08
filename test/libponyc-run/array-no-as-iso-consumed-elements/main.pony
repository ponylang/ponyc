trait iso T
class iso C1 is T
class iso C2 is T
class iso C3 is T

primitive Foo
  fun apply(): Array[(C1 | C2 | C3)] =>
    let c1: C1 iso = C1
    let c2: C2 iso = C2
    let c3: C3 iso = C3
    [consume c1; consume c2; consume c3]

actor Main
  new create(env: Env) =>
    None

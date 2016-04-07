trait T
  fun foo() =>
    let x: U32 = 0

class Baz is T
class Bar is T

actor Main
  new create(env: Env) => None

actor Main
  new create(env: Env) =>
    A.create().f()

class A
  fun f() =>
    let a = create() // issue #4259 compiler crash

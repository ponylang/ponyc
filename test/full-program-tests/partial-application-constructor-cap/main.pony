use @pony_exitcode[None](code: I32)

class Bar
  new create() => None

actor Main
  new create(env: Env) =>
    let mk = Bar~create()
    let b = mk()

    let mk_val: {(): Bar ref^} val = Bar~create()
    let b2 = mk_val()

    this.take_factory(Bar~create())

  be take_factory(factory: {(): Bar ref^} val) =>
    let b = factory()
    @pony_exitcode(42)

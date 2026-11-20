actor \nodoc\ Main
  new create(env: Env) =>
    eligible()
    parameter(1)
    local()
    nopoll()
    one()
    conditional()
    helper()
    helper()

  be eligible() =>
    helper()
    helper()

  be parameter(n: USize) =>
    helper()
    helper()

  be local() =>
    let n: USize = 1
    helper()
    helper()

  be \noalloc\ nopoll() =>
    helper()
    helper()

  be one() => helper()

  be conditional() =>
    if true then helper() end
    helper()

  fun helper() => None

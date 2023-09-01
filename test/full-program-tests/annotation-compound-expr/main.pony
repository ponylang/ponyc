class C
  fun apply() =>
    let foo = true
    let bar = true
    let x = true

    if \a, b\ foo then
      None
    elseif \a, b\ bar then
      None
    else \a, b\
      None
    end

    ifdef \a, b\ "foo" then
      None
    elseif \a, b\ "bar" then
      None
    else \a, b\
      None
    end

    while \a, b\ foo do
      None
    else \a, b\
      None
    end

    repeat \a, b\
      None
    until \a, b\ foo else \a, b\
      None
    end

    match \a, b\ x
    | \a, b\ foo => None
    else \a, b\
      None
    end

    try \a, b\
      error
    else \a, b\
      None
    then \a, b\
      None
    end

    recover \a, b\ foo end

actor Main
  new create(env: Env) =>
    None

actor Main
  new create(env: Env) =>
    try
      foo()
    end

  fun foo() ? =>
    bar()

  fun bar() ? =>
    try
      error
    end

    error
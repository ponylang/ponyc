actor Main
  new create(env: Env) =>
    foo("hello")

  fun foo(s: String) =>
    try
      one(s)
      error
    else
      one(s)
    end

  fun one(s: String) =>
    s.clone()

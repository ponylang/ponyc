actor Main
  let _out: OutStream

  new create(env: Env) =>
    _out = env.out
    foo("hello")

  fun foo(s: String) =>
    try
      one("h")
      error
    else
      one("e")
    end

  fun one(s: String) =>
    _out.print(s)

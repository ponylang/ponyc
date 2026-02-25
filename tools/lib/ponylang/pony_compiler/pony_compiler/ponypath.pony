use "cli"
primitive PonyPath
  fun apply(env: Env): (String | None) =>
    let env_vars = EnvVars(env.vars)
    try
      env_vars("PONYPATH")?
    end

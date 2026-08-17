use "cli"

primitive PonyPath
  """
  Extracts the PONYPATH environment variable value.
  """
  fun apply(env: Env): (String | None) =>
    let env_vars = EnvVars(env.vars)
    try
      env_vars("PONYPATH")?
    end

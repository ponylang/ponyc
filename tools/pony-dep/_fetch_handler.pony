actor _FetchHandler is FetchNotify
  """
  CLI result handler for the fetch subcommand. Translates fetch outcomes
  into stderr messages and exit codes.
  """
  let _env: Env

  new create(env: Env) =>
    _env = env

  be fetch_failed(err: FetchError) =>
    _env.err.print("error: " + err.message)
    _env.exitcode(1)

  be fetch_succeeded() =>
    None

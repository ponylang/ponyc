actor _ResolveHandler is ResolveNotify
  """
  CLI result handler for the fetch subcommand. Prints errors to stderr
  and sets the exit code. Silent on success.
  """
  let _env: Env

  new create(env: Env) =>
    _env = env

  be resolve_failed(message: String val) =>
    _env.err.print("error: " + message)
    _env.exitcode(1)

  be resolve_complete(
    errors: Array[(String val, String val)] val,
    fetched: USize,
    skipped: USize)
  =>
    for (name, message) in errors.values() do
      _env.err.print("error: " + name + ": " + message)
    end
    if errors.size() > 0 then
      _env.exitcode(1)
    end

actor _FetchAllHandler is FetchAllNotify
  """
  CLI result handler for the fetch subcommand. Prints errors to stderr
  and sets the exit code. Silent on success.
  """
  let _env: Env

  new create(env: Env) =>
    _env = env

  be fetch_all_failed(message: String val) =>
    _env.err.print("error: " + message)
    _env.exitcode(1)

  be fetch_all_complete(
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

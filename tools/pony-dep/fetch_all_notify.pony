interface tag FetchAllNotify
  """
  Callback for a `FetchAll` operation over a `pony.deps` file.
  """

  be fetch_all_failed(message: String val)
    """
    The operation cannot start: the config file is unreadable, fails to
    parse, or contains an unsupported dep type.
    """

  be fetch_all_complete(
    errors: Array[(String val, String val)] val,
    fetched: USize,
    skipped: USize)
    """
    All dependencies have been processed. Each element of `errors` is a
    `(dep_name, message)` pair for a dependency that failed. `fetched`
    is the number placed successfully, `skipped` the number whose target
    directory already existed.
    """

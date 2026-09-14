interface tag AddNotify
  """
  Receives the result of an `Add` operation.
  """

  be add_failed(message: String val)
    """
    Called when the add operation fails at any stage.
    """

  be add_succeeded()
    """
    Called when the dependency has been fetched, hashed, and recorded in
    the configuration file.
    """

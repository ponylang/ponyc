interface val _PropertyLogger
  """
  Receives log messages from property-based tests.
  """
  fun log(msg: String, verbose: Bool = false)

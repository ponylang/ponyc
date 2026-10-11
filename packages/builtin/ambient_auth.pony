primitive AmbientAuth
  """
  This type represents the root capability. When a Pony program starts, the
  Env passed to the Main actor contains an instance of the root capability.

  Ambient access to the root capability is denied outside of the builtin
  module. Inside the builtin module, only Env creates a Root.

  The root capability can be used by any module that wants to establish a
  principle of least authority. A typical usage is to have a parameter on a
  constructor for some resource that expects a limiting capability specific to
  the module, but will also accept the root capability as representing
  unlimited access.
  """
  new _create() =>
    None

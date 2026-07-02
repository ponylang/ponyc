primitive _Prim
  """
  Fixture for verifying that synthetic compiler-added methods (eq/ne from
  add_comparable) produce no spurious hints alongside user-defined methods.
  """
  // hints: " box" (receiver), " val" (name: String), " val" (None return)
  fun greet(name: String): None =>
    None

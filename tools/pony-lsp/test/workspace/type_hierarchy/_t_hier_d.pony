trait _THierD
  """
  Supertype half of a cross-file pair (subtype: _THierE in _t_hier_e.pony).
  Subtypes should return [_THierE] even though it is defined in a separate
  file, demonstrating that the server walks all modules in the compiled package.
  """
  fun f_d(): None

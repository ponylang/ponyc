primitive DoNotOptimise
  """
  Contains functions preventing some compiler optimisations, namely dead code
  removal. This is useful for benchmarking purposes.
  """

  fun apply[A](obj: A) =>
    """
    Prevent the compiler from optimising out obj and any computation it is
    derived from. This doesn't prevent constant propagation.
    """
    compile_intrinsic

  fun observe() =>
    """
    Prevent the compiler from optimising out writes to an object marked by
    the apply function.
    """
    compile_intrinsic

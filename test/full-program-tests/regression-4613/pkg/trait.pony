trait T
  fun _override(): (T | None)

  fun _with_default() =>
    """
    Will compile if #4613 remains fixed. Otherwise, this will fail to
    compiled IFF a class from outside this package implements this trait
    and inherits this default method
    """
    match _override()
    | let t: T =>
      t._with_default()
    | None =>
      None
    end

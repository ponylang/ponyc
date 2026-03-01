trait T
  fun _override(): (T | None)

  fun _with_default() =>
    """
    Will compile if #4613 remains fixed. Otherwise, this will fail to
    compiled IFF a class from outside this package implements this trait
    and inherits this default method
    """
    match \exhaustive\ _override()
    | let t: T =>
      t._with_default()
    | None =>
      None
    end

  fun _get_opc(): OPC

  fun _calling_method_that_is_private_to_this_package_on_another_type() =>
    """
    Will compile if #4613 remains fixed. Otherwise, this will fail to
    compiled IFF a class from outside this package implements this trait
    and inherits this default method
    """
    _get_opc()._private()

class OPC
  new create() =>
    None

  fun _private() =>
    None

class _ReceiverCapability
  """
  Demonstrates that hover displays receiver capabilities in method signatures.
  """

  fun box boxed_method(): String =>
    """
    A boxed method - receiver capability is 'box'.
    Hover shows 'fun box boxed_method(): String' with the receiver capability.
    The receiver capability indicates how 'this' can be accessed in the method.
    """
    "box"

  fun val valued_method(): String =>
    """
    A val method - receiver capability is 'val'.
    Hover shows 'fun val valued_method(): String' with the receiver capability.
    """
    "val"

  fun ref mutable_method() =>
    """
    A ref method - receiver capability is 'ref'.
    Hover shows 'fun ref mutable_method()' with the receiver capability.
    """
    None

class _Function
  """
  Demonstrates that hover works on function calls.
  Hovering over a method name in a call shows its signature and docstring.
  """

  fun demo_function_calls(): String =>
    """
    Try hovering over the method names in the function calls below.
    Hover will show the method signature with parameters and docstring.
    """
    let result1 = this.helper_method("test")
    let result2 = this.method_with_multiple_params(42, "hello", true)
    result1 + result2

  fun helper_method(input: String): String =>
    """
    A helper method that processes input.
    """
    input.upper()

  fun method_with_multiple_params(x: U32, y: String, flag: Bool): String =>
    """
    Method with multiple parameters for testing.
    """
    y

  fun ref mutable_method(value: U32) =>
    """
    A method with a ref receiver capability.
    """
    None

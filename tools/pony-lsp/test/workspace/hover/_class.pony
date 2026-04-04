class _Class
  """
  A simple class for exercising LSP hover.
  """
  let field_name: String
  var mutable_field: U32
  embed embedded_field: Array[String] = Array[String]

  new create(name: String) =>
    field_name = name
    mutable_field = 0

  fun simple_method(): String =>
    """
    Returns the field name.
    """
    field_name

  fun method_with_params(x: U32, y: String): Bool =>
    """
    A method with parameters.
    """
    x > 0

  fun ref update_field(count: USize) =>
    """
    Update the mutable field.
    """
    mutable_field = count.u32()

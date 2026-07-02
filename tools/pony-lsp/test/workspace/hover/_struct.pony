struct _Struct
  """
  A simple struct for exercising LSP hover.
  """
  var x: I32
  var y: I32

  new create(x': I32, y': I32) =>
    x = x'
    y = y'

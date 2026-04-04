actor _Actor
  """
  A simple actor for exercising LSP hover.
  """
  let _name: String

  new create(name': String) =>
    _name = name'

  be do_something(value: U64) =>
    None

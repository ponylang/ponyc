class ref _CapKeyword
  """
  Tests that hover is suppressed on capability keyword positions.
  Covers entity cap, field type cap, receiver caps, parameter type cap,
  return type cap, and local variable type cap.
  """

  let _data: String val

  new ref create() =>
    _data = ""

  fun val recv_val(): String val =>
    _data

  fun ref recv_ref(x: String val): None =>
    None

  fun box arrow_type(x: box->String): None =>
    None

  fun local_cap() =>
    let local: String val = ""
    None

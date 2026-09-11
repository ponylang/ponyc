use "pony_check"

class \nodoc\ iso _PropertyHeadersCaseInsensitive
  is Property1[(String val, String val)]
  """
  Adding a header and retrieving it with a different case variant returns
  the same value.
  """
  fun name(): String => "headers/case_insensitive"

  fun gen(): Generator[(String val, String val)] =>
    Generators.zip2[String val, String val](
      Generators.ascii_letters(1, 20),
      Generators.ascii_printable(1, 50))

  fun ref property(
    arg1: (String val, String val),
    ph: PropertyHelper)
  =>
    (let header_name, let value) = arg1
    let headers = Headers
    headers.add(header_name, value)

    // Retrieve with different case variants
    ph.assert_eq[String val](
      value, _get_or_empty(headers, header_name.upper()))
    ph.assert_eq[String val](
      value, _get_or_empty(headers, header_name.lower()))

  fun _get_or_empty(headers: Headers box, header_name: String)
    : String val
  =>
    match headers.get(header_name)
    | let v: String val => v
    else
      ""
    end

class \nodoc\ iso _PropertyHeadersSetReplaces
  is Property1[(String val, String val, String val)]
  """
  Calling `set()` twice for the same name keeps only the second value.
  """
  fun name(): String => "headers/set_replaces"

  fun gen(): Generator[(String val, String val, String val)] =>
    Generators.zip3[String val, String val, String val](
      Generators.ascii_letters(1, 20),
      Generators.ascii_printable(1, 50),
      Generators.ascii_printable(1, 50))

  fun ref property(
    arg1: (String val, String val, String val),
    ph: PropertyHelper)
  =>
    (let header_name, let v1, let v2) = arg1
    let headers = Headers
    headers.set(header_name, v1)
    headers.set(header_name, v2)

    ph.assert_eq[USize](1, headers.size())
    ph.assert_eq[String val](v2, _get_or_empty(headers, header_name))

  fun _get_or_empty(headers: Headers box, header_name: String)
    : String val
  =>
    match headers.get(header_name)
    | let v: String val => v
    else
      ""
    end

class \nodoc\ iso _PropertyHeadersAddPreserves
  is Property1[(String val, String val, String val)]
  """
  Calling `add()` twice for the same name keeps both values. `get()` returns
  the first, and `size()` reflects both entries.
  """
  fun name(): String => "headers/add_preserves"

  fun gen(): Generator[(String val, String val, String val)] =>
    Generators.zip3[String val, String val, String val](
      Generators.ascii_letters(1, 20),
      Generators.ascii_printable(1, 50),
      Generators.ascii_printable(1, 50))

  fun ref property(
    arg1: (String val, String val, String val),
    ph: PropertyHelper)
  =>
    (let header_name, let v1, let v2) = arg1
    let headers = Headers
    headers.add(header_name, v1)
    headers.add(header_name, v2)

    ph.assert_eq[USize](2, headers.size())
    // get() returns the first value added
    ph.assert_eq[String val](v1, _get_or_empty(headers, header_name))

  fun _get_or_empty(headers: Headers box, header_name: String)
    : String val
  =>
    match headers.get(header_name)
    | let v: String val => v
    else
      ""
    end

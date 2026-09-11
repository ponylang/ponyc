class Headers
  """
  A collection of HTTP headers with case-insensitive name lookup.

  Names are lowercased on storage. Use `set()` to replace all values for a
  name, or `add()` to append an additional value (appropriate for multi-value
  headers like Set-Cookie).
  """
  embed _headers: Array[(String, String)]

  new create() =>
    """
    Create an empty header collection.
    """
    _headers = Array[(String, String)]

  fun ref set(name: String, value: String) =>
    """
    Set a header, removing any existing entries with the same name.

    After this call, `get(name)` returns `value` and there is exactly one
    entry for this name.
    """
    let lower_name: String val = name.lower()
    var i: USize = 0
    while i < _headers.size() do
      try
        if _headers(i)?._1 == lower_name then
          _headers.delete(i)?
        else
          i = i + 1
        end
      else
        i = i + 1
      end
    end
    _headers.push((lower_name, value))

  fun ref add(name: String, value: String) =>
    """
    Add a header entry without removing existing entries with the same name.

    This is appropriate for headers that can appear multiple times
    (e.g., Set-Cookie). Use `set()` when you want to replace.
    """
    _headers.push((name.lower(), value))

  fun get(name: String): (String | None) =>
    """
    Get the first value for the given header name (case-insensitive).

    Returns `None` if no header with that name exists.
    """
    let lower_name: String val = name.lower()
    for (n, v) in _headers.values() do
      if n == lower_name then return v end
    end
    None

  fun size(): USize =>
    """
    Return the number of header entries.
    """
    _headers.size()

  fun values(): ArrayValues[(String, String), this->Array[(String, String)]] =>
    """
    Iterate over all header entries as (name, value) pairs.
    """
    _headers.values()

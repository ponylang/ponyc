primitive ParseFormURLEncoded
  """
  Parse an `application/x-www-form-urlencoded` string into a `FormURLEncoded`
  collection.

  Splits on `&`, then on the first `=`. Decodes `+` as space and
  percent-decodes both keys and values.

  Duplicate keys are preserved in order. Keys without `=` get an
  empty-string value.

  The `;` separator (from an older HTML convention) is not supported —
  it was dropped from the WHATWG URL Standard and is rarely encountered
  in practice.

  This works on any `application/x-www-form-urlencoded` string — URI query
  components, HTTP POST request bodies, or any other source. The input
  should not include a leading `?` (that delimiter is not part of the
  encoded data).
  """
  fun apply(input: String val)
    : (FormURLEncoded val | InvalidPercentEncoding val)
  =>
    """
    Parse `input` into decoded key-value pairs.
    """
    if input.size() == 0 then
      return FormURLEncoded(
        recover val Array[(String val, String val)](0) end)
    end

    // Count pairs for pre-allocation
    var count: USize = 1
    for c in input.values() do
      if c == '&' then count = count + 1 end
    end

    let pairs = Array[(String val, String val)](count)
    var start: USize = 0
    var i: USize = 0

    while i <= input.size() do
      let at_end = i == input.size()
      let at_amp = try not at_end and (input(i)? == '&') else false end

      if at_end or at_amp then
        let pair_str: String val =
          input.substring(start.isize(), i.isize())
        match \exhaustive\ _parse_pair(pair_str)
        | (let k: String val, let v: String val) => pairs.push((k, v))
        | let err: InvalidPercentEncoding val => return err
        end
        start = i + 1
      end
      i = i + 1
    end

    let result: Array[(String val, String val)] iso =
      recover iso
        Array[(String val, String val)](pairs.size())
      end
    for pair in pairs.values() do
      result.push(pair)
    end
    FormURLEncoded(consume result)

  fun _parse_pair(pair: String val)
    : ((String val, String val) | InvalidPercentEncoding val)
  =>
    // Find first '='
    var eq_pos: USize = pair.size() // sentinel: no '=' found
    var j: USize = 0
    while j < pair.size() do
      try
        if pair(j)? == '=' then
          eq_pos = j
          break
        end
      else
        _Unreachable()
      end
      j = j + 1
    end

    let raw_key: String val =
      if eq_pos < pair.size() then
        pair.substring(0, eq_pos.isize())
      else
        pair
      end

    let raw_value: String val =
      if eq_pos < pair.size() then
        pair.substring((eq_pos + 1).isize(), pair.size().isize())
      else
        ""
      end

    let key_plus: String val = _plus_to_space(raw_key)
    let value_plus: String val = _plus_to_space(raw_value)

    match \exhaustive\ PercentDecode(key_plus)
    | let key: String val =>
      match \exhaustive\ PercentDecode(value_plus)
      | let value: String val => (key, value)
      | let err: InvalidPercentEncoding val => err
      end
    | let err: InvalidPercentEncoding val => err
    end

  fun _plus_to_space(input: String val): String val =>
    if not input.contains("+") then
      return input
    end
    let out = String(input.size())
    for c in input.values() do
      if c == '+' then
        out.push(' ')
      else
        out.push(c)
      end
    end
    out.clone()

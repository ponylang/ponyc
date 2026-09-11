primitive _PercentEncoder
  """
  RFC 3986 and WHATWG percent-encoding for query strings and form data.

  Two encoding modes:
  - `query()`: RFC 3986 unreserved characters pass through, everything else
    is `%XX`, spaces become `%20`.
  - `form()`: WHATWG form encoding, spaces become `+`, unreserved characters
    pass through, everything else is `%XX`.
  """

  fun query(input: String): String iso^ =>
    """
    Encode a string using RFC 3986 percent-encoding for query components.

    Unreserved characters (`A-Z a-z 0-9 - . _ ~`) pass through unchanged.
    All other bytes are encoded as `%XX`.
    """
    let buf = recover iso String(input.size()) end
    for byte in input.values() do
      if _is_rfc3986_unreserved(byte) then
        buf.push(byte)
      else
        buf .> push('%')
          .> push(_hex_digit(byte >> 4))
          .push(_hex_digit(byte and 0x0F))
      end
    end
    consume buf

  fun form(input: String): String iso^ =>
    """
    Encode a string using WHATWG application/x-www-form-urlencoded encoding.

    Characters `A-Z a-z 0-9 * - . _` pass through unchanged. Spaces become
    `+`. All other bytes are encoded as `%XX`.
    """
    let buf = recover iso String(input.size()) end
    for byte in input.values() do
      if byte == ' ' then
        buf.push('+')
      elseif _is_form_unreserved(byte) then
        buf.push(byte)
      else
        buf .> push('%')
          .> push(_hex_digit(byte >> 4))
          .push(_hex_digit(byte and 0x0F))
      end
    end
    consume buf

  fun _is_rfc3986_unreserved(byte: U8): Bool =>
    ((byte >= 'A') and (byte <= 'Z')) or
      ((byte >= 'a') and (byte <= 'z')) or
      ((byte >= '0') and (byte <= '9')) or
      (byte == '-') or (byte == '.') or (byte == '_') or (byte == '~')

  fun _is_form_unreserved(byte: U8): Bool =>
    ((byte >= 'A') and (byte <= 'Z')) or
      ((byte >= 'a') and (byte <= 'z')) or
      ((byte >= '0') and (byte <= '9')) or
      (byte == '*') or (byte == '-') or (byte == '.') or (byte == '_')

  fun _hex_digit(nibble: U8): U8 =>
    let n = nibble and 0x0F
    if n < 10 then '0' + n
    else ('A' - 10) + n
    end

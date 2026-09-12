primitive _PctEncode
  """
  Percent-encodes a string value per RFC 6570 encoding rules.

  Two modes are supported:
  - Unreserved mode (`allow_reserved = false`): only unreserved characters
    (A-Za-z0-9 - . _ ~) pass through; everything else is pct-encoded.
  - Reserved mode (`allow_reserved = true`): additionally passes through
    reserved characters (: / ? # [ ] @ ! $ & ' ( ) * + , ; =) and
    existing pct-encoded triplets (%XX).
  """
  fun encode(value: String, allow_reserved: Bool): String iso^ =>
    let out = recover iso String(value.size()) end
    var i: USize = 0
    while i < value.size() do
      try
        let byte = value(i)?
        if _is_unreserved(byte) then
          out.push(byte)
          i = i + 1
        elseif allow_reserved and _is_reserved(byte) then
          out.push(byte)
          i = i + 1
        elseif allow_reserved and (byte == '%') and
          ((i + 2) < value.size()) and
          _is_hex_digit(value(i + 1)?) and
          _is_hex_digit(value(i + 2)?)
        then
          // Pass through existing pct-encoded triplet
          out .> push(byte) .> push(value(i + 1)?)
            .> push(value(i + 2)?)
          i = i + 3
        else
          out .> push('%') .> push(_hex_digit(byte >> 4))
            .> push(_hex_digit(byte and 0x0F))
          i = i + 1
        end
      else
        _Unreachable()
        break
      end
    end
    consume out

  fun _is_unreserved(byte: U8): Bool =>
    // A-Za-z0-9 - . _ ~
    ((byte >= 'A') and (byte <= 'Z')) or
      ((byte >= 'a') and (byte <= 'z')) or
      ((byte >= '0') and (byte <= '9')) or
      (byte == '-') or (byte == '.') or (byte == '_') or (byte == '~')

  fun _is_reserved(byte: U8): Bool =>
    // : / ? # [ ] @ ! $ & ' ( ) * + , ; =
    match byte
    | ':' | '/' | '?' | '#' | '[' | ']' | '@' => true
    | '!' | '$' | '&' | '\'' | '(' | ')' | '*' | '+' | ',' | ';' | '=' =>
      true
    else
      false
    end

  fun _is_hex_digit(byte: U8): Bool =>
    ((byte >= '0') and (byte <= '9')) or
      ((byte >= 'A') and (byte <= 'F')) or
      ((byte >= 'a') and (byte <= 'f'))

  fun _hex_digit(nibble: U8): U8 =>
    if nibble < 10 then
      '0' + nibble
    else
      'A' + (nibble - 10)
    end

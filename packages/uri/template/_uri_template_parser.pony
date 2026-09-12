use "collections"

primitive _URITemplateParser
  """
  Single-pass left-to-right parser for RFC 6570 URI templates.

  Scans the template string, producing an array of `_TemplatePart` values
  (literals and expressions) on success, or a `URITemplateParseError`
  describing the first syntax error encountered.
  """
  fun parse(template: String)
    : (Array[_TemplatePart] val | URITemplateParseError)
  =>
    let parts: Array[_TemplatePart] iso =
      recover iso Array[_TemplatePart] end
    var i: USize = 0

    while i < template.size() do
      try
        if template(i)? == '{' then
          match \exhaustive\ _parse_expression(template, i)
          | (let expr: _Expression, let next: USize) =>
            parts.push(expr)
            i = next
          | let err: URITemplateParseError =>
            return err
          end
        else
          match \exhaustive\ _parse_literal(template, i)
          | (let lit: _Literal, let next: USize) =>
            parts.push(lit)
            i = next
          | let err: URITemplateParseError =>
            return err
          end
        end
      else
        _Unreachable()
        return URITemplateParseError("internal error", i)
      end
    end

    consume parts

  fun _parse_literal(template: String, start: USize)
    : ((_Literal, USize) | URITemplateParseError)
  =>
    var i = start
    while i < template.size() do
      try
        let byte = template(i)?
        if byte == '{' then
          break
        elseif byte == '}' then
          return URITemplateParseError("unexpected '}'", i)
        elseif not _is_literal_char(byte) then
          // Check for pct-encoded triplet
          if (byte == '%') and ((i + 2) < template.size()) and
            _PctEncode._is_hex_digit(template(i + 1)?) and
            _PctEncode._is_hex_digit(template(i + 2)?)
          then
            i = i + 3
          else
            return URITemplateParseError("invalid literal character", i)
          end
        else
          i = i + 1
        end
      else
        _Unreachable()
        return URITemplateParseError("internal error", i)
      end
    end

    if i > start then
      (_Literal(template.substring(start.isize(), i.isize())), i)
    else
      // Should not happen — caller checks for '{' before calling
      _Unreachable()
      URITemplateParseError("internal error", start)
    end

  fun _is_literal_char(byte: U8): Bool =>
    """
    Check whether a byte is allowed in literal text per RFC 6570 Section 2.1.

    Allowed: 0x21, 0x23-0x24, 0x26, 0x28-0x3B, 0x3D, 0x3F-0x5B, 0x5D,
    0x5F, 0x61-0x7A, 0x7E, and bytes >= 0x80 (non-ASCII).
    The '%' character is handled separately (pct-encoded triplets).
    """
    if byte >= 0x80 then
      // Non-ASCII bytes (ucschar, iprivate) are allowed
      true
    elseif byte == 0x21 then true // !
    elseif (byte >= 0x23) and (byte <= 0x24) then true // # $
    elseif byte == 0x26 then true // &
    elseif (byte >= 0x28) and (byte <= 0x3B) then true // ()*+,-./ 0-9:;
    elseif byte == 0x3D then true // =
    elseif (byte >= 0x3F) and (byte <= 0x5B) then true // ? @ A-Z [
    elseif byte == 0x5D then true // ]
    elseif byte == 0x5F then true // _
    elseif (byte >= 0x61) and (byte <= 0x7A) then true // a-z
    elseif byte == 0x7E then true // ~
    else false
    end

  fun _parse_expression(template: String, start: USize)
    : ((_Expression, USize) | URITemplateParseError)
  =>
    // start points at '{'
    var i = start + 1

    if i >= template.size() then
      return URITemplateParseError("unclosed expression", start)
    end

    // Check for operator
    let op: _Operator =
      try
        match template(i)?
        | '+' => i = i + 1; _OpPlus
        | '#' => i = i + 1; _OpHash
        | '.' => i = i + 1; _OpDot
        | '/' => i = i + 1; _OpSlash
        | ';' => i = i + 1; _OpSemicolon
        | '?' => i = i + 1; _OpQuestion
        | '&' => i = i + 1; _OpAmpersand
        | '=' => return URITemplateParseError(
            "reserved operator '='", start + 1)
        | ',' => return URITemplateParseError(
            "reserved operator ','", start + 1)
        | '!' => return URITemplateParseError(
            "reserved operator '!'", start + 1)
        | '@' => return URITemplateParseError(
            "reserved operator '@'", start + 1)
        | '|' => return URITemplateParseError(
            "reserved operator '|'", start + 1)
        | '}' => return URITemplateParseError(
            "empty expression", start)
        else
          _OpNone
        end
      else
        _Unreachable()
        return URITemplateParseError("internal error", i)
      end

    // Parse comma-separated varspecs
    let varspecs: Array[_VarSpec] iso =
      recover iso Array[_VarSpec] end
    var first = true

    while i < template.size() do
      try
        if template(i)? == '}' then
          if first then
            return URITemplateParseError("empty expression", start)
          end
          return (_Expression(op, consume varspecs), i + 1)
        end

        if not first then
          if template(i)? != ',' then
            return URITemplateParseError("expected ',' or '}'", i)
          end
          i = i + 1
          if (i >= template.size()) then
            return URITemplateParseError("unclosed expression", start)
          end
          try
            if template(i)? == '}' then
              return URITemplateParseError("empty varname", i)
            end
          else
            _Unreachable()
            return URITemplateParseError("internal error", i)
          end
        end

        match \exhaustive\ _parse_varspec(template, i)
        | (let vs: _VarSpec, let next: USize) =>
          varspecs.push(vs)
          i = next
          first = false
        | let err: URITemplateParseError =>
          return err
        end
      else
        _Unreachable()
        return URITemplateParseError("internal error", i)
      end
    end

    URITemplateParseError("unclosed expression", start)

  fun _parse_varspec(template: String, start: USize)
    : ((_VarSpec, USize) | URITemplateParseError)
  =>
    // Parse varname
    match \exhaustive\ _parse_varname(template, start)
    | (let name: String val, let i: USize) =>
      // Check for modifier
      if i >= template.size() then
        return URITemplateParseError(
          "unclosed expression",
          _find_open_brace(template, start))
      end
      try
        match template(i)?
        | ':' =>
          // Prefix modifier
          match \exhaustive\ _parse_prefix(template, i + 1)
          | (let max_len: USize, let next: USize) =>
            (_VarSpec(name, _ModPrefix(max_len)), next)
          | let err: URITemplateParseError =>
            err
          end
        | '*' =>
          (_VarSpec(name, _ModExplode), i + 1)
        else
          (_VarSpec(name), i)
        end
      else
        _Unreachable()
        URITemplateParseError("internal error", i)
      end
    | let err: URITemplateParseError =>
      err
    end

  fun _parse_varname(template: String, start: USize)
    : ((String val, USize) | URITemplateParseError)
  =>
    var i = start
    var after_dot = false

    // First character must be a varchar (not a dot)
    if i >= template.size() then
      return URITemplateParseError("empty varname", start)
    end

    try
      if not _is_varchar_start(template(i)?) then
        if template(i)? == '.' then
          return URITemplateParseError("leading dot in varname", i)
        end
        return URITemplateParseError("invalid varname character", i)
      end
    else
      _Unreachable()
      return URITemplateParseError("internal error", i)
    end

    // Consume varchar characters with optional dots between segments
    while i < template.size() do
      try
        let byte = template(i)?
        if _is_varchar(byte) then
          if byte == '%' then
            // Must be a pct-encoded triplet
            if ((i + 2) < template.size()) and
              _PctEncode._is_hex_digit(template(i + 1)?) and
              _PctEncode._is_hex_digit(template(i + 2)?)
            then
              i = i + 3
            else
              return URITemplateParseError(
                "invalid pct-encoded triplet in varname", i)
            end
          else
            i = i + 1
          end
          after_dot = false
        elseif byte == '.' then
          if after_dot then
            return URITemplateParseError(
              "consecutive dots in varname", i)
          end
          after_dot = true
          i = i + 1
        else
          break
        end
      else
        _Unreachable()
        return URITemplateParseError("internal error", i)
      end
    end

    if after_dot then
      return URITemplateParseError("trailing dot in varname", i - 1)
    end

    if i == start then
      return URITemplateParseError("empty varname", start)
    end

    (template.substring(start.isize(), i.isize()), i)

  fun _is_varchar_start(byte: U8): Bool =>
    _is_varchar(byte)

  fun _is_varchar(byte: U8): Bool =>
    // A-Za-z0-9 _ and % (for pct-encoded)
    ((byte >= 'A') and (byte <= 'Z')) or
      ((byte >= 'a') and (byte <= 'z')) or
      ((byte >= '0') and (byte <= '9')) or
      (byte == '_') or
      (byte == '%')

  fun _parse_prefix(template: String, start: USize)
    : ((USize, USize) | URITemplateParseError)
  =>
    var i = start
    var digits: USize = 0
    var value: USize = 0

    while i < template.size() do
      try
        let byte = template(i)?
        if (byte >= '0') and (byte <= '9') then
          digits = digits + 1
          if digits > 4 then
            return URITemplateParseError(
              "prefix length exceeds 4 digits", start)
          end
          value = (value * 10) + (byte - '0').usize()
          i = i + 1
        else
          break
        end
      else
        _Unreachable()
        return URITemplateParseError("internal error", i)
      end
    end

    if digits == 0 then
      return URITemplateParseError("prefix length missing", start)
    end

    if value == 0 then
      return URITemplateParseError(
        "prefix length must be at least 1",
        start)
    end

    if value > 9999 then
      return URITemplateParseError("prefix length exceeds 9999", start)
    end

    (value, i)

  fun _find_open_brace(template: String, near: USize): USize =>
    """
    Find the nearest preceding '{' for error reporting.
    """
    var i = near
    while i > 0 do
      i = i - 1
      try
        if template(i)? == '{' then
          return i
        end
      end
    end
    near

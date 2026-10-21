primitive _URITemplateExpander
  """
  Expands a parsed URI template against a set of variable bindings.

  Walks the parsed parts array and produces the expanded URI string
  per RFC 6570 Section 3.
  """
  fun expand(
    parts: Array[_TemplatePart] val,
    vars: URITemplateVariables box)
    : String iso^
  =>
    let out = String
    for part in parts.values() do
      match \exhaustive\ part
      | let lit: _Literal =>
        out.append(_PctEncode.encode(lit.text, true))
      | let expr: _Expression =>
        _expand_expression(expr, vars, out)
      end
    end
    out.clone()

  fun _expand_expression(
    expr: _Expression,
    vars: URITemplateVariables box,
    out: String ref)
  =>
    let op = expr.operator
    let props = _OperatorProperties
    var first_defined = true

    for varspec in expr.varspecs.values() do
      match vars._get(varspec.name)
      | let s: String val =>
        if first_defined then
          out.append(props.first(op))
          first_defined = false
        else
          out.append(props.sep(op))
        end
        _expand_string(s, varspec, op, out)
      | let list: Array[String val] val =>
        if list.size() == 0 then
          // Empty list treated as undefined
          continue
        end
        if first_defined then
          out.append(props.first(op))
          first_defined = false
        else
          out.append(props.sep(op))
        end
        _expand_list(list, varspec, op, out)
      | let pairs: Array[(String val, String val)] val =>
        if pairs.size() == 0 then
          // Empty pairs treated as undefined
          continue
        end
        if first_defined then
          out.append(props.first(op))
          first_defined = false
        else
          out.append(props.sep(op))
        end
        _expand_pairs(pairs, varspec, op, out)
      end
    end

  fun _expand_string(
    value: String val,
    varspec: _VarSpec,
    op: _Operator,
    out: String ref)
  =>
    let props = _OperatorProperties
    let allow_reserved = props.allow_reserved(op)

    let actual_value =
      match varspec.modifier
      | let prefix: _ModPrefix =>
        _prefix_characters(value, prefix.max_length, allow_reserved)
      else
        value
      end

    if props.named(op) then
      out.append(_PctEncode.encode(varspec.name, false))
      if actual_value.size() == 0 then
        out.append(props.ifemp(op))
      else
        out.push('=')
        out.append(_PctEncode.encode(actual_value, allow_reserved))
      end
    else
      out.append(_PctEncode.encode(actual_value, allow_reserved))
    end

  fun _expand_list(
    list: Array[String val] val,
    varspec: _VarSpec,
    op: _Operator,
    out: String ref)
  =>
    let props = _OperatorProperties
    let allow_reserved = props.allow_reserved(op)
    let is_named = props.named(op)

    match varspec.modifier
    | _ModExplode =>
      for (idx, item) in list.pairs() do
        if idx > 0 then
          out.append(props.sep(op))
        end
        if is_named then
          out.append(_PctEncode.encode(varspec.name, false))
          if item.size() == 0 then
            out.append(props.ifemp(op))
          else
            out.push('=')
            out.append(_PctEncode.encode(item, allow_reserved))
          end
        else
          out.append(_PctEncode.encode(item, allow_reserved))
        end
      end
    else
      // No modifier or prefix (prefix has no special effect on lists)
      if is_named then
        out.append(_PctEncode.encode(varspec.name, false))
        // Non-explode list always uses '=' separator between name and
        // comma-joined values. Empty lists are already excluded as
        // undefined before reaching this code.
        out.push('=')
      end
      for (idx, item) in list.pairs() do
        if idx > 0 then
          out.push(',')
        end
        out.append(_PctEncode.encode(item, allow_reserved))
      end
    end

  fun _expand_pairs(
    pairs: Array[(String val, String val)] val,
    varspec: _VarSpec,
    op: _Operator,
    out: String ref)
  =>
    let props = _OperatorProperties
    let allow_reserved = props.allow_reserved(op)
    let is_named = props.named(op)

    match varspec.modifier
    | _ModExplode =>
      for (idx, (key, value)) in pairs.pairs() do
        if idx > 0 then
          out.append(props.sep(op))
        end
        out.append(_PctEncode.encode(key, allow_reserved))
        if is_named and (value.size() == 0) then
          out.append(props.ifemp(op))
        else
          out.push('=')
          out.append(_PctEncode.encode(value, allow_reserved))
        end
      end
    else
      // No modifier or prefix
      if is_named then
        out.append(_PctEncode.encode(varspec.name, false))
        out.push('=')
      end
      for (idx, (key, value)) in pairs.pairs() do
        if idx > 0 then
          out.push(',')
        end
        out.append(_PctEncode.encode(key, allow_reserved))
        out.push(',')
        out.append(_PctEncode.encode(value, allow_reserved))
      end
    end

  fun _prefix_characters(
    value: String,
    max_length: USize,
    allow_reserved: Bool)
    : String val
  =>
    """
    Return the first `max_length` characters of `value`.

    RFC 6570 2.4.1: the prefix length counts characters, not octets, "in
    order to avoid splitting ... within a pct-encoded triplet." Under
    reserved expansion (`+` and `#`), an existing `%XX` triplet in the
    value counts as one character.
    """
    var byte_offset: USize = 0
    var char_count: USize = 0

    while (char_count < max_length) and (byte_offset < value.size()) do
      try
        if allow_reserved and (value(byte_offset)? == '%') and
          ((byte_offset + 2) < value.size()) and
          _PctEncode._is_hex_digit(value(byte_offset + 1)?) and
          _PctEncode._is_hex_digit(value(byte_offset + 2)?)
        then
          byte_offset = byte_offset + 3
        else
          (_, let len) = value.utf32(byte_offset.isize())?
          byte_offset = byte_offset + len.usize()
        end
        char_count = char_count + 1
      else
        break
      end
    end

    if byte_offset >= value.size() then
      value
    else
      value.substring(0, byte_offset.isize())
    end

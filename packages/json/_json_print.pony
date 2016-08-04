primitive _JsonPrint
  fun _indent(
    buf: String iso, indent: String, level': USize): String iso^
  =>
    """
    Add indentation to the buf to the appropriate indent_level
    """
    var level = level'

    buf.push('\n')

    while level != 0 do
      buf.append(indent)
      level = level - 1
    end

    consume buf

  fun _string(d: box->JsonType, buf': String iso, indent: String, level: USize,
    pretty: Bool): String iso^
  =>
    """
    Generate string representation of the given data.
    """
    var buf = consume buf'

    match d
    | let x: Bool => buf.append(x.string())
    | let x: None => buf.append("null")
    | let x: String =>
      buf = _escaped_string(consume buf, x)

    | let x: JsonArray box =>
      buf = x._show(consume buf, indent, level, pretty)

    | let x: JsonObject box =>
      buf = x._show(consume buf, indent, level, pretty)

    | let x': I64 =>
      var x = if x' < 0 then
        buf.push('-')
        -x'
      else
        x'
      end

      if x == 0 then
        buf.push('0')
      else
        // Append the numbers in reverse order
        var i = buf.size()

        while x != 0 do
          buf.push((x % 10).u8() or 48)
          x = x / 10
        end

        var j = buf.size() - 1

        // Place the numbers back in the proper order
        try
          while i < j do
            buf(i) = buf(j = j - 1) = buf(i = i + 1)
          end
        end
      end

    | let x: F64 =>
      // Make sure our printed floats can be distinguished from integers
      let basic = x.string()

      if basic.count(".") == 0 then
        buf.append(consume basic)
        buf.append(".0")
      else
        buf.append(consume basic)
      end
    else
      // Can never happen
      ""
    end

    buf

  fun _escaped_string(buf: String iso, s: String): String iso^ =>
    """
    Generate a version of the given string with escapes for all non-printable
    and non-ASCII characters.
    """
    var i: USize = 0

    buf.push('"')

    try
      while i < s.size() do
        (let c, let count) = s.utf32(i.isize())
        i = i + count.usize()

        if c == '"' then
          buf.append("\\\"")
        elseif c == '\\' then
          buf.append("\\\\")
        elseif c == '\b' then
          buf.append("\\b")
        elseif c == '\f' then
          buf.append("\\f")
        elseif c == '\t' then
          buf.append("\\t")
        elseif c == '\r' then
          buf.append("\\r")
        elseif c == '\n' then
          buf.append("\\n")
        elseif (c >= 0x20) and (c < 0x80) then
          buf.push(c.u8())
        elseif c < 0x10000 then
          buf.append("\\u")
          let fmt = FormatSettingsInt.set_format(FormatHexBare).set_width(4)
            .set_fill('0')
          buf.append(c.string(fmt))
        else
          let high = (((c - 0x10000) >> 10) and 0x3FF) + 0xD800
          let low = ((c - 0x10000) and 0x3FF) + 0xDC00
          let fmt = FormatSettingsInt.set_format(FormatHexBare).set_width(4)
          buf.append("\\u")
          buf.append(high.string(fmt))
          buf.append("\\u")
          buf.append(low.string(fmt))
        end
      end
    end

    buf.push('"')
    buf

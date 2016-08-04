primitive _JsonPrint
  fun _indent(
    text: String iso, indent: String, level': USize): String iso^
  =>
    """
    Add indentation to the text to the appropriate indent_level
    """
    var level = level'

    text.push('\n')

    while level != 0 do
      text.append(indent)
      level = level - 1
    end

    consume text

  fun _string(d: box->JsonType, text': String iso, indent: String, level: USize,
    pretty: Bool): String iso^
  =>
    """
    Generate string representation of the given data.
    """
    var text = consume text'

    match d
    | let x: Bool => text.append(x.string())
    | let x: None => text.append("null")
    | let x: String =>
      text = _escaped_string(consume text, x)

    | let x: JsonArray box =>
      text = x._show(consume text, indent, level, pretty)

    | let x: JsonObject box =>
      text = x._show(consume text, indent, level, pretty)

    | let x': I64 =>
      var x = if x' < 0 then
        text.push('-')
        -x'
      else
        x'
      end

      if x == 0 then
        text.push('0')
      else
        // Append the numbers in reverse order
        var i = text.size()

        while x != 0 do
          text.push((x % 10).u8() or 48)
          x = x / 10
        end

        var j = text.size() - 1

        // Place the numbers back in the proper order
        try
          while i < j do
            text(i) = text(j = j - 1) = text(i = i + 1)
          end
        end
      end

    | let x: F64 =>
      // Make sure our printed floats can be distinguished from integers
      let basic = x.string()

      if basic.count(".") == 0 then
        text.append(consume basic)
        text.append(".0")
      else
        text.append(consume basic)
      end
    else
      // Can never happen
      ""
    end

    text

  fun _escaped_string(text: String iso, s: String): String iso^ =>
    """
    Generate a version of the given string with escapes for all non-printable
    and non-ASCII characters.
    """
    var i: USize = 0

    text.push('"')

    try
      while i < s.size() do
        (let c, let count) = s.utf32(i.isize())
        i = i + count.usize()

        if c == '"' then
          text.append("\\\"")
        elseif c == '\\' then
          text.append("\\\\")
        elseif c == '\b' then
          text.append("\\b")
        elseif c == '\f' then
          text.append("\\f")
        elseif c == '\t' then
          text.append("\\t")
        elseif c == '\r' then
          text.append("\\r")
        elseif c == '\n' then
          text.append("\\n")
        elseif (c >= 0x20) and (c < 0x80) then
          text.push(c.u8())
        elseif c < 0x10000 then
          text.append("\\u")
          let fmt = FormatSettingsInt.set_format(FormatHexBare).set_width(4)
            .set_fill('0')
          text.append(c.string(fmt))
        else
          let high = (((c - 0x10000) >> 10) and 0x3FF) + 0xD800
          let low = ((c - 0x10000) and 0x3FF) + 0xDC00
          let fmt = FormatSettingsInt.set_format(FormatHexBare).set_width(4)
          text.append("\\u")
          text.append(high.string(fmt))
          text.append("\\u")
          text.append(low.string(fmt))
        end
      end
    end

    text.push('"')
    text

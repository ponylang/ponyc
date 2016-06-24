primitive _JsonPrint
  fun _string(d: box->JsonType, indent: String,
    pretty_print: Bool = true): String
  =>
    """
    Generate string representation of the given data.
    """
    match d
    | let x: I64 => x.string()
    | let x: Bool => x.string()
    | let x: None => "null"
    | let x: String => _escaped_string(x)
    | let x: JsonArray box => x.string(pretty_print, indent)
    | let x: JsonObject box => x.string(pretty_print, indent)
    | let x: F64 =>
      // Make sure our printed floats can be distinguished from integers
      let basic = x.string()
      if basic.count(".") == 0 then basic + ".0" else basic end
    else
      // Can never happen
      ""
    end

  fun _escaped_string(s: String): String =>
    """
    Generate a version of the given string with escapes for all non-printable
    and non-ASCII characters.
    """
    var out = recover ref String end
    var i: USize = 0

    try
      while i < s.size() do
        (let c, let count) = s.utf32(i.isize())
        i = i + count.usize()

        if c == '"' then
          out.append("\\\"")
        elseif c == '\\' then
          out.append("\\\\")
        elseif c == '\b' then
          out.append("\\b")
        elseif c == '\f' then
          out.append("\\f")
        elseif c == '\t' then
          out.append("\\t")
        elseif c == '\r' then
          out.append("\\r")
        elseif c == '\n' then
          out.append("\\n")
        elseif (c >= 0x20) and (c < 0x80) then
          out.push(c.u8())
        elseif c < 0x10000 then
          out.append("\\u")
          let fmt = FormatSettingsInt.set_format(FormatHexBare).set_width(4)
            .set_fill('0')
          out.append(c.string(fmt))
        else
          let high = (((c - 0x10000) >> 10) and 0x3FF) + 0xD800
          let low = ((c - 0x10000) and 0x3FF) + 0xDC00
          let fmt = FormatSettingsInt.set_format(FormatHexBare).set_width(4)
          out.append("\\u")
          out.append(high.string(fmt))
          out.append("\\u")
          out.append(low.string(fmt))
        end
      end
    end

    "\"" + out + "\""

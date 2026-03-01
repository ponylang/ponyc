primitive _JsonPrint
  """Private serialization helper. Handles all JsonValue members."""

  fun compact(value: JsonValue): String iso^ =>
    """Compact serialization of any JSON value."""
    _value(recover iso String(256) end, value, "", 0, false)

  fun pretty(value: JsonValue, indent: String = "  "): String iso^ =>
    """Pretty-printed serialization of any JSON value."""
    _value(recover iso String(256) end, value, indent, 0, true)

  fun compact_object(obj: JsonObject box): String iso^ =>
    """Compact serialization from a box reference (for Stringable)."""
    _object(recover iso String(256) end, obj, "", 0, false)

  fun pretty_object(obj: JsonObject box, indent: String = "  "): String iso^ =>
    """Pretty-printed serialization from a box reference."""
    _object(recover iso String(256) end, obj, indent, 0, true)

  fun compact_array(arr: JsonArray box): String iso^ =>
    """Compact serialization from a box reference (for Stringable)."""
    _array(recover iso String(256) end, arr, "", 0, false)

  fun pretty_array(arr: JsonArray box, indent: String = "  "): String iso^ =>
    """Pretty-printed serialization from a box reference."""
    _array(recover iso String(256) end, arr, indent, 0, true)

  fun _value(
    buf: String iso,
    value: JsonValue,
    indent: String,
    level: USize,
    is_pretty: Bool)
    : String iso^
  =>
    match value
    | let obj: JsonObject =>
      _object(consume buf, obj, indent, level, is_pretty)
    | let arr: JsonArray =>
      _array(consume buf, arr, indent, level, is_pretty)
    | let s: String => _escaped_string(consume buf, s)
    | let n: I64 =>
      buf.append(n.string())
      consume buf
    | let n: F64 => _float(consume buf, n)
    | let b: Bool =>
      buf.append(if b then "true" else "false" end)
      consume buf
    | None =>
      buf.append("null")
      consume buf
    end

  fun _object(
    buf: String iso,
    obj: JsonObject box,
    indent: String,
    level: USize,
    is_pretty: Bool)
    : String iso^
  =>
    var b = consume buf
    b.push('{')
    if obj.size() == 0 then
      b.push('}')
      return consume b
    end

    var first = true
    for (k, v) in obj.pairs() do
      if not first then b.push(',') end
      first = false
      if is_pretty then
        b.push('\n')
        b = _indent(consume b, indent, level + 1)
      end
      b = _escaped_string(consume b, k)
      b.push(':')
      if is_pretty then b.push(' ') end
      b = _value(consume b, v, indent, level + 1, is_pretty)
    end

    if is_pretty then
      b.push('\n')
      b = _indent(consume b, indent, level)
    end
    b.push('}')
    consume b

  fun _array(
    buf: String iso,
    arr: JsonArray box,
    indent: String,
    level: USize,
    is_pretty: Bool)
    : String iso^
  =>
    var b = consume buf
    b.push('[')
    if arr.size() == 0 then
      b.push(']')
      return consume b
    end

    var first = true
    for v in arr.values() do
      if not first then b.push(',') end
      first = false
      if is_pretty then
        b.push('\n')
        b = _indent(consume b, indent, level + 1)
      end
      b = _value(consume b, v, indent, level + 1, is_pretty)
    end

    if is_pretty then
      b.push('\n')
      b = _indent(consume b, indent, level)
    end
    b.push(']')
    consume b

  fun _escaped_string(buf: String iso, s: String box): String iso^ =>
    var b = consume buf
    b.push('"')
    for byte in s.values() do
      match \exhaustive\ byte
      | '"'  => b.append("\\\"")
      | '\\' => b.append("\\\\")
      | 0x08 => b.append("\\b")
      | 0x0C => b.append("\\f")
      | '\n' => b.append("\\n")
      | '\r' => b.append("\\r")
      | '\t' => b.append("\\t")
      | let c: U8 if c < 0x20 =>
        b.append("\\u00")
        b.push(_hex_char((c >> 4) and 0x0F))
        b.push(_hex_char(c and 0x0F))
      else
        b.push(byte)
      end
    end
    b.push('"')
    consume b

  fun _float(buf: String iso, n: F64): String iso^ =>
    let s: String = n.string()
    buf.append(s)
    if
      (not s.contains("."))
        and (not s.contains("e"))
        and (not s.contains("E"))
        and (not s.contains("inf"))
        and (not s.contains("nan"))
    then
      buf.append(".0")
    end
    consume buf

  fun _indent(buf: String iso, indent: String, level: USize): String iso^ =>
    var i: USize = 0
    while i < level do
      buf.append(indent)
      i = i + 1
    end
    consume buf

  fun _hex_char(nibble: U8): U8 =>
    if nibble < 10 then '0' + nibble
    else 'a' + (nibble - 10)
    end

primitive _JsonPrint
  """
  Private serialization helper. Handles all JsonValue members.

  Serialization is iterative, not recursive: an explicit stack of container
  frames bounds nesting depth by the heap instead of the scheduler thread's
  native stack, which deeply nested input would otherwise overflow. The output
  buffer is a single `String iso` threaded through the walk.
  """

  fun compact(value: JsonValue): String iso^ =>
    """Compact serialization of any JSON value."""
    let stack = Array[(_PrintObjectFrame | _PrintArrayFrame)]
    _run(_open(recover iso String(256) end, value, 0, stack), "", false, stack)

  fun pretty(value: JsonValue, indent: String = "  "): String iso^ =>
    """Pretty-printed serialization of any JSON value."""
    let stack = Array[(_PrintObjectFrame | _PrintArrayFrame)]
    _run(_open(recover iso String(256) end, value, 0, stack), indent, true,
      stack)

  fun compact_object(obj: JsonObject box): String iso^ =>
    """Compact serialization from a box reference (backs JsonObject.print)."""
    let stack = Array[(_PrintObjectFrame | _PrintArrayFrame)]
    _run(_seed_object(recover iso String(256) end, obj, 0, stack), "", false,
      stack)

  fun pretty_object(obj: JsonObject box, indent: String = "  "): String iso^ =>
    """Pretty-printed serialization from a box reference."""
    let stack = Array[(_PrintObjectFrame | _PrintArrayFrame)]
    _run(_seed_object(recover iso String(256) end, obj, 0, stack), indent, true,
      stack)

  fun compact_array(arr: JsonArray box): String iso^ =>
    """Compact serialization from a box reference (backs JsonArray.print)."""
    let stack = Array[(_PrintObjectFrame | _PrintArrayFrame)]
    _run(_seed_array(recover iso String(256) end, arr, 0, stack), "", false,
      stack)

  fun pretty_array(arr: JsonArray box, indent: String = "  "): String iso^ =>
    """Pretty-printed serialization from a box reference."""
    let stack = Array[(_PrintObjectFrame | _PrintArrayFrame)]
    _run(_seed_array(recover iso String(256) end, arr, 0, stack), indent, true,
      stack)

  fun _run(
    buf: String iso,
    indent: String,
    is_pretty: Bool,
    stack: Array[(_PrintObjectFrame | _PrintArrayFrame)])
    : String iso^
  =>
    """
    Drive the work stack until every open container is closed. Each iteration
    advances the top frame by one element (opening any child container, which
    pushes a new top frame — depth-first) or, when the frame is exhausted,
    writes its closing bracket and pops it.
    """
    var b = consume buf
    while stack.size() > 0 do
      let frame =
        try
          stack(stack.size() - 1)?
        else
          _Unreachable()
          return recover iso String end
        end
      match \exhaustive\ frame
      | let f: _PrintObjectFrame =>
        if f.iter.has_next() then
          (let k, let v) =
            try f.iter.next()? else _Unreachable(); ("", None) end
          if not f.first then b.push(',') end
          f.first = false
          if is_pretty then
            b.push('\n')
            b = _indent(consume b, indent, f.level + 1)
          end
          b = _escaped_string(consume b, k)
          b.push(':')
          if is_pretty then b.push(' ') end
          b = _open(consume b, v, f.level + 1, stack)
        else
          if is_pretty then
            b.push('\n')
            b = _indent(consume b, indent, f.level)
          end
          b.push('}')
          try stack.pop()? else _Unreachable() end
        end
      | let f: _PrintArrayFrame =>
        if f.iter.has_next() then
          let v = try f.iter.next()? else _Unreachable(); None end
          if not f.first then b.push(',') end
          f.first = false
          if is_pretty then
            b.push('\n')
            b = _indent(consume b, indent, f.level + 1)
          end
          b = _open(consume b, v, f.level + 1, stack)
        else
          if is_pretty then
            b.push('\n')
            b = _indent(consume b, indent, f.level)
          end
          b.push(']')
          try stack.pop()? else _Unreachable() end
        end
      end
    end
    consume b

  fun _open(
    buf: String iso,
    value: JsonValue,
    level: USize,
    stack: Array[(_PrintObjectFrame | _PrintArrayFrame)])
    : String iso^
  =>
    """
    Append a scalar, or open a container: write its opening bracket and, when
    non-empty, push a frame for `_run` to fill. Empty containers are closed
    inline and never push a frame.
    """
    match \exhaustive\ value
    | let obj: JsonObject => _seed_object(consume buf, obj, level, stack)
    | let arr: JsonArray => _seed_array(consume buf, arr, level, stack)
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

  fun _seed_object(
    buf: String iso,
    obj: JsonObject box,
    level: USize,
    stack: Array[(_PrintObjectFrame | _PrintArrayFrame)])
    : String iso^
  =>
    """Write `{`; close an empty object inline, else push a frame for `_run`."""
    var b = consume buf
    b.push('{')
    if obj.size() == 0 then
      b.push('}')
    else
      stack.push(_PrintObjectFrame(obj, level))
    end
    consume b

  fun _seed_array(
    buf: String iso,
    arr: JsonArray box,
    level: USize,
    stack: Array[(_PrintObjectFrame | _PrintArrayFrame)])
    : String iso^
  =>
    """Write `[`; close an empty array inline, else push a frame for `_run`."""
    var b = consume buf
    b.push('[')
    if arr.size() == 0 then
      b.push(']')
    else
      stack.push(_PrintArrayFrame(arr, level))
    end
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

class _PrintObjectFrame
  """
  A JSON object being serialized: its pair iterator, indent level, and
  whether its first entry has been written yet.
  """
  let iter: Iterator[(String, JsonValue)]
  let level: USize
  var first: Bool = true

  new create(obj: JsonObject box, level': USize) =>
    iter = obj.pairs()
    level = level'

class _PrintArrayFrame
  """
  A JSON array being serialized: its value iterator, indent level, and
  whether its first element has been written yet.
  """
  let iter: Iterator[JsonValue]
  let level: USize
  var first: Bool = true

  new create(arr: JsonArray box, level': USize) =>
    iter = arr.values()
    level = level'

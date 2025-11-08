use "collections"

type JsonType is (F64 | I64 | Bool | None | String | JsonArray | JsonObject)
  """
  All JSON data types.
  """

class val JsonArray
  let data: Array[JsonType] val
    """
    The actual array containing JSON structures.
    """

  new val create(data': Array[JsonType] val) =>
    """
    Create a Json array from an actual array.
    """
    data = data'

  new val empty() =>
    data = []

  fun string(indent: String = "", pretty_print: Bool = false): String =>
    """
    Generate string representation of this array.
    """
    let buf = _show(recover String(256) end, indent, 0, pretty_print)
    buf.compact()
    buf

  fun _show(
    buf': String iso,
    indent: String = "",
    level: USize,
    pretty: Bool)
    : String iso^
  =>
    """
    Append the string representation of this array to the provided String.
    """
    var buf = consume buf'

    if data.size() == 0 then
      buf.append("[]")
      return buf
    end

    buf.push('[')

    var print_comma = false

    for v in data.values() do
      if print_comma then
        buf.push(',')
      else
        print_comma = true
      end

      if pretty then
        buf = _JsonPrint._indent(consume buf, indent, level + 1)
      end

      buf = _JsonPrint._string(v, consume buf, indent, level + 1, pretty)
    end

    if pretty then
      buf = _JsonPrint._indent(consume buf, indent, level)
    end

    buf.push(']')
    buf


class val JsonObject
  let data: Map[String, JsonType] val
    """
    The actual JSON object structure,
    mapping `String` keys to other JSON structures.
    """

  new val create(data': Map[String, JsonType] val) =>
    """
    Create a Json object from a map.
    """
    data = data'

  new val empty() =>
    data = recover val Map[String, JsonType](0) end

  fun string(indent: String = "", pretty_print: Bool = false): String =>
    """
    Generate string representation of this object.
    """
    let buf = _show(recover String(256) end, indent, 0, pretty_print)
    buf.compact()
    buf

  fun _show(buf': String iso, indent: String = "", level: USize, pretty: Bool)
    : String iso^
  =>
    """
    Append the string representation of this object to the provided String.
    """
    var buf = consume buf'

    if data.size() == 0 then
      buf.append("{}")
      return buf
    end

    buf.push('{')

    var print_comma = false

    for (k, v) in data.pairs() do
      if print_comma then
        buf.push(',')
      else
        print_comma = true
      end

      if pretty then
        buf = _JsonPrint._indent(consume buf, indent, level + 1)
      end

      buf.push('"')
      buf.append(k)

      if pretty then
        buf.append("\": ")
      else
        buf.append("\":")
      end

      buf = _JsonPrint._string(v, consume buf, indent, level + 1, pretty)
    end

    if pretty then
      buf = _JsonPrint._indent(consume buf, indent, level)
    end

    buf.push('}')
    buf

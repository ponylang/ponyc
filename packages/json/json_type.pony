use "collections"

type JSONType is (F64 | I64 | Bool | None | String | JSONArray | JSONObject)
  """
  All JSON data types.
  """

class JSONArray
  var data: Array[JSONType]

  new iso create(len: USize = 0) =>
    """
    Create an array with zero elements, but space for len elements.
    """
    data = Array[JSONType](len)

  new from_array(data': Array[JSONType]) =>
    """
    Create a JSON array from an actual array.
    """
    data = data'

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
        buf = _JSONPrint._indent(consume buf, indent, level + 1)
      end

      buf = _JSONPrint._string(v, consume buf, indent, level + 1, pretty)
    end

    if pretty then
      buf = _JSONPrint._indent(consume buf, indent, level)
    end

    buf.push(']')
    buf


class JSONObject
  var data: Map[String, JSONType]

  new iso create(prealloc: USize = 6) =>
    """
    Create a map with space for prealloc elements without triggering a
    resize. Defaults to 6.
    """
    data = Map[String, JSONType](prealloc)

  new from_map(data': Map[String, JSONType]) =>
    """
    Create a JSON object from a map.
    """
    data = data'

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
        buf = _JSONPrint._indent(consume buf, indent, level + 1)
      end

      buf.push('"')
      buf.append(k)

      if pretty then
        buf.append("\": ")
      else
        buf.append("\":")
      end

      buf = _JSONPrint._string(v, consume buf, indent, level + 1, pretty)
    end

    if pretty then
      buf = _JSONPrint._indent(consume buf, indent, level)
    end

    buf.push('}')
    buf

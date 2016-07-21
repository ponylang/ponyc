use "collections"

type JsonType is (F64 | I64 | Bool | None | String | JsonArray | JsonObject)
  """
  All JSON data types.
  """

class JsonArray
  var data: Array[JsonType]

  new iso create(len: USize = 0) =>
    """
    Create an array with zero elements, but space for len elements.
    """
    data = Array[JsonType](len)

  new from_array(data': Array[JsonType]) =>
    """
    Create a Json array from an actual array.
    """
    data = data'

  fun string(indent: String = "", pretty_print: Bool = false): String =>
    """
    Generate string representation of this array.
    """
    if data.size() == 0 then
      return "[]"
    end

    var text = recover String(256) end
    let elem_indent = indent + "  "

    text.append("[")

    for i in data.values() do
      if text.size() > 1 then
        text.append(",")
      end

      if pretty_print then
        text.append("\n")
        text.append(elem_indent)
      end

      text.append(_JsonPrint._string(i, elem_indent, pretty_print))
    end

    if pretty_print then
      text.append("\n")
      text.append(indent)
    end

    text.append("]")
    text

class JsonObject
  var data: Map[String, JsonType]

  new iso create(prealloc: USize = 6) =>
    """
    Create a map with space for prealloc elements without triggering a
    resize. Defaults to 6.
    """
    data = Map[String, JsonType](prealloc)

  new from_map(data': Map[String, JsonType]) =>
    """
    Create a Json object from a map.
    """
    data = data'

  fun string(indent: String = "", pretty_print: Bool = false): String =>
    """
    Generate string representation of this object.
    """
    if data.size() == 0 then
      return "{}"
    end

    var text = recover String(256) end
    let elem_indent = indent + "  "

    text.append("{")

    for i in data.pairs() do
      if text.size() > 1 then
        text.append(",")
      end

      if pretty_print then
        text.append("\n")
        text.append(elem_indent)
      end

      text.append("\"")
      text.append(i._1)
      text.append("\":")

      if pretty_print then
        text.append(" ")
      end

      text.append(_JsonPrint._string(i._2, elem_indent, pretty_print))
    end

    if pretty_print then
      text.append("\n")
      text.append(indent)
    end

    text.append("}")
    text

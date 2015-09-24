use "collections"


type JsonType is (F64 | I64 | Bool | None | String | JsonArray | JsonObject)
  """
  All JSON data types.
  """


class JsonArray
  var data: Array[JsonType]

  new iso create(len: U64 = 0) =>
    """
    Create an array with zero elements, but space for len elements.
    """
    data = Array[JsonType](len)

  fun string(indent: String = ""): String =>
    """
    Generate string representation of this array.
    """
    if data.size() == 0 then
      return "[]"
    end

    var text = ""
    let elem_indent = indent + "  "

    for i in data.values() do
      if text.size() > 0 then
        text = text + ","
      end

      text = text + "\n" + elem_indent + _JsonPrint._string(i, elem_indent)
    end
    "[" + text + "\n" + indent + "]"


class JsonObject
  var data: Map[String, JsonType]
  
  new iso create(prealloc: U64 = 6) =>
    """
    Create a map with space for prealloc elements without triggering a
    resize. Defaults to 6.
    """
    data = Map[String, JsonType](prealloc)

  fun string(indent: String = ""): String =>
    """
    Generate string representation of this object.
    """
    if data.size() == 0 then
      return "{}"
    end

    var text = ""
    let elem_indent = indent + "  "

    for i in data.pairs() do
      if text.size() > 0 then
        text = text + ","
      end
      
      text = text + "\n" + elem_indent + "\"" + i._1 + "\": " +
        _JsonPrint._string(i._2, elem_indent)
    end
    "{" + text + "\n" + indent + "}"


class JsonDoc
  """
  Top level JSON type containing an entire document.
  A JSON document consists of exactly 1 value.
  """
  var data: JsonType

  new iso create() =>
    """
    Default constructor building a document containing a single null.
    """
    data = None

  fun string(): String =>
    """
    Generate string representation of this document.
    """
    _JsonPrint._string(data, "")


primitive _JsonPrint
  fun _string(d: box->JsonType, indent: String): String =>
    """
    Generate string representation of the given data.
    """
    match d
    | let x: I64 => x.string()
    | let x: Bool => x.string()
    | let x: None => "null"
    | let x: String => _escaped_string(x)
    | let x: JsonArray => x.string(indent)
    | let x: JsonObject => x.string(indent)
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
    var i: U64 = 0

    try
      while i < s.size() do
        (let c, let count) = s.utf32(i.i64())
        i = i + count.u64()

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
          out.append(c.string(FormatHexBare where width = 4, fill = '0'))
        else
          let high = (((c - 0x10000) >> 10) and 0x3FF) + 0xD800
          let low = ((c - 0x10000) and 0x3FF) + 0xDC00
          out.append("\\u")
          out.append(high.string(FormatHexBare where width = 4))
          out.append("\\u")
          out.append(low.string(FormatHexBare where width = 4))
        end
      end
    end

    "\"" + out + "\""

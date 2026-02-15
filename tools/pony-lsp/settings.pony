use "json"

class val Settings
  """
  Settings that are expected to arrive from the client.

  Expected format:

  ```json
  {
    "defines": ["DEFINE1", "DEFINE2"],
    "ponypath": "/foo/bar:/baz"
  }
  ```
  """
  let _defines: Array[String] val
    """
    Userflags, usually defined by `-D` when calling ponyc.
    """

  let _ponypath: (String | None)
    """
    Additional ponypath entries, that will be added to the
    entries that pony-lsp adds on its own:

      1. the stdlib path
      2. path list extracted from the `PONYPATH` environment variable
    """

  new val from_json(data: JsonObject) =>
    let defs = recover trn Array[String].create(4) end
      try
        let json_arr = JsonPathParser.compile("$.defines")?.query_one(data) as JsonArray
        for elem in json_arr.values() do
          try
            defs.push(elem as String)
          end
        end
      else
        JsonArray
      end
    this._defines = consume defs

    this._ponypath =
      try
        JsonPathParser.compile("$.ponypath")?.query_one(data) as String
      end

  fun val to_json(): JsonObject =>
    var defs = JsonArray
    for def in this._defines.values() do
      defs = defs.push(def)
    end
    JsonObject
      .update("defines", defs)
      .update("ponypath", this._ponypath)

  fun val defines(): Array[String] val =>
    this._defines

  fun val ponypath(): String =>
    match this._ponypath
    | let s: String => s
    | None => ""
    end

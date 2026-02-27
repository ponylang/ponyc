use "json"
use pc = "collections/persistent"

class val Settings
  """
  Settings that are expected to arrive from the client.

  Expected format:

  ```json
  {
    "defines": ["DEFINE1", "DEFINE2"],
    "ponypath": ["/foo/bar", "/baz"]
  }
  ```
  """
  let _defines: Array[String] val
    """
    Userflags, usually defined by `-D` when calling ponyc.
    """

  let _ponypath: Array[String] val
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
        let pony_path_json = JsonPathParser.compile("$.ponypath")?.query_one(data)
        match pony_path_json
        | let arr: JsonArray =>
          let pony_path_array = recover trn Array[String].create(arr.size()) end
          for json_entry in arr.values() do
            try
              pony_path_array.push(json_entry as String)
            end
          end
          consume pony_path_array
        | let pony_path_str: String =>
          [pony_path_str]
        else
          []
        end
      else
        []
      end

  fun val to_json(): JsonObject =>
    var defs = JsonArray
    for def in this._defines.values() do
      defs = defs.push(def)
    end
    var pp = JsonArray(pc.Vec[JsonValue].concat(defs.values()))
    JsonObject
      .update("defines", defs)
      .update("ponypath", pp)

  fun val defines(): Array[String] val =>
    this._defines

  fun val ponypath(): Array[String] val =>
    this._ponypath

use "immutable-json"
use "files"

class val ServerOptions
  """
  server options provided via Initialize request
  """
  let pony_path: (Array[String] val | None)

  new val from_json(json: JsonObject) =>
    pony_path = 
      try
        recover val
          Path.split_list(json.data("PONYPATH")? as String val)
        end
      end
    
  new val default() =>
    pony_path = None

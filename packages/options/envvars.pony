use "collections"

primitive EnvVars
  fun apply(from: Array[String] val): Map[String, String] val =>
    """
    Turns an array of strings that look like environment variables, ie
    key=value, into a map from string to string.
    """
    let map = recover Map[String, String](from.size()) end

    try
      for kv in from.values() do
        try
          let delim = kv.find("=")
          let k = kv.substring(0, delim - 1)
          let v = kv.substring(delim + 1, -1)
          map(consume k) = consume v
        else
          map(kv) = ""
        end
      end
    end

    map

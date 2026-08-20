use json = "json"

actor Main
  new create(env: Env) =>
    env.out.print("=== Building JSON ===")
    _build_example(env)
    env.out.print("")

    env.out.print("=== Serializing (JSONPrinter) ===")
    _printer_example(env)
    env.out.print("")

    env.out.print("=== Parsing JSON ===")
    _parse_example(env)
    env.out.print("")

    env.out.print("=== Navigation (JSONNav) ===")
    _nav_example(env)
    env.out.print("")

    env.out.print("=== Lenses (JSONLens) ===")
    _lens_example(env)
    env.out.print("")

    env.out.print("=== Modification via Lens ===")
    _lens_modify_example(env)
    env.out.print("")

    env.out.print("=== JSONPath Queries ===")
    _jsonpath_example(env)
    env.out.print("")

    env.out.print("=== JSONPath Filters ===")
    _jsonpath_filter_example(env)
    env.out.print("")

    env.out.print("=== JSONPath Function Extensions ===")
    _jsonpath_function_example(env)

  fun _build_example(env: Env) =>
    let doc = json.JSONObject
      .update("name", "Alice")
      .update("age", I64(30))
      .update("active", true)
      .update(
        "tags",
        json.JSONArray
          .push("admin")
          .push("developer"))
      .update(
        "address",
        json.JSONObject
          .update("city", "Portland")
          .update("state", "OR"))

    env.out.print("Compact: " + doc.print())
    env.out.print("Pretty:")
    env.out.print(doc.pretty_print())

  fun _printer_example(env: Env) =>
    // JSONPrinter.print serializes any JSONValue — objects, arrays, and
    // scalars alike — which is what JSONObject.print()/JSONArray.print()
    // cannot do on their own.
    let doc = json.JSONObject
      .update("ok", true)
      .update("count", I64(3))
      .update("ratio", F64(1.5))
      .update("note", None)
      .update("items", json.JSONArray.push("a").push("b"))

    env.out.print("Whole value: " + json.JSONPrinter.print(doc))

    // Scalars and JSON null serialize correctly on their own.
    env.out.print("Bool:   " + json.JSONPrinter.print(true))
    env.out.print("Int:    " + json.JSONPrinter.print(I64(42)))
    env.out.print("Float:  " + json.JSONPrinter.print(F64(2)))
    env.out.print("String: " + json.JSONPrinter.print("hi\"there"))
    env.out.print("Null:   " + json.JSONPrinter.print(None))

    env.out.print("Pretty:")
    env.out.print(json.JSONPrinter.pretty(doc))

  fun _parse_example(env: Env) =>
    let source =
      """
      {"users":[{"name":"Bob","age":25},{"name":"Carol","age":35}],"count":2}
      """

    match \exhaustive\ json.JSONParser.parse(source)
    | let j: json.JSONValue =>
      env.out.print("Parsed successfully")
      match j
      | let obj: json.JSONObject => env.out.print("Root is object with " +
        obj.size().string() + " keys")
      end
    | let err: json.JSONParseError =>
      env.out.print("Parse error: " + err.string())
    end

  fun _nav_example(env: Env) =>
    let source =
      """
      {"users":[{"name":"Bob","age":25},{"name":"Carol","age":35}],"count":2}
      """

    match \exhaustive\ json.JSONParser.parse(source)
    | let j: json.JSONValue =>
      let nav = json.JSONNav(j)
      try
        let first_name = nav("users")(USize(0))("name").as_string()?
        let first_age = nav("users")(USize(0))("age").as_i64()?
        env.out.print("First user: " + first_name +
          ", age " + first_age.string())
      else
        env.out.print("Navigation failed")
      end

      try
        let count = nav("count").as_i64()?
        env.out.print("Count: " + count.string())
      end

      // JSONNotFound propagation — no crash, just JSONNotFound at the end
      let missing = nav("nonexistent")("deep")("path")
      env.out.print("Missing path found? " + missing.found().string())

    | let err: json.JSONParseError =>
      env.out.print("Parse error: " + err.string())
    end

  fun _lens_example(env: Env) =>
    let source =
      """
      {"config":{"database":{"host":"localhost","port":5432},"debug":false}}
      """

    match \exhaustive\ json.JSONParser.parse(source)
    | let j: json.JSONValue =>
      // Read via lens
      let host_lens = json.JSONLens("config")("database")("host")
      match \exhaustive\ host_lens.get(j)
      | let host: json.JSONValue =>
        env.out.print("Host: " + json.JSONPrinter.print(host))
      | json.JSONNotFound =>
        env.out.print("Host not found")
      end

      // Composed lens
      let db_lens = json.JSONLens("config")("database")
      let port_lens = db_lens.compose(json.JSONLens("port"))
      match \exhaustive\ port_lens.get(j)
      | let port: json.JSONValue =>
        env.out.print("Port: " + json.JSONPrinter.print(port))
      | json.JSONNotFound =>
        env.out.print("Port not found")
      end

    | let err: json.JSONParseError =>
      env.out.print("Parse error: " + err.string())
    end

  fun _lens_modify_example(env: Env) =>
    let source =
      """
      {"config":{"database":{"host":"localhost","port":5432},"debug":false}}
      """

    match \exhaustive\ json.JSONParser.parse(source)
    | let j: json.JSONValue =>
      // Modify a deeply nested value
      let host_lens = json.JSONLens("config")("database")("host")
      match \exhaustive\ host_lens.set(j, "prod.example.com")
      | let updated: json.JSONValue =>
        match updated
        | let obj: json.JSONObject =>
          env.out.print("After host change:")
          env.out.print(obj.pretty_print())
        end
      | json.JSONNotFound =>
        env.out.print("Could not set host — path not found")
      end

      // Remove a value
      let debug_lens = json.JSONLens("config")("debug")
      match \exhaustive\ debug_lens.remove(j)
      | let updated: json.JSONValue =>
        match updated
        | let obj: json.JSONObject =>
          env.out.print("After removing debug:")
          env.out.print(obj.pretty_print())
        end
      | json.JSONNotFound =>
        env.out.print("Could not remove debug — path not found")
      end

    | let err: json.JSONParseError =>
      env.out.print("Parse error: " + err.string())
    end

  fun _jsonpath_example(env: Env) =>
    let source =
      """
      {
        "store": {
          "book": [
            {"title": "Sayings",
             "author": "Rees", "price": 8.95},
            {"title": "Sword",
             "author": "Waugh", "price": 12.99},
            {"title": "Moby Dick",
             "author": "Melville", "price": 8.99}
          ],
          "bicycle": {"color": "red", "price": 399}
        }
      }
      """

    match \exhaustive\ json.JSONParser.parse(source)
    | let doc: json.JSONValue =>
      // Wildcard: all authors
      match \exhaustive\ json.JSONPathParser.parse("$.store.book[*].author")
      | let path: json.JSONPath =>
        let authors = path.query(doc)
        env.out.print("Authors: " + _format_results(authors))
      | let err: json.JSONPathParseError =>
        env.out.print("Path error: " + err.string())
      end

      // Recursive descent: all prices in the store
      try
        let price_path = json.JSONPathParser.compile("$.store..price")?
        let prices = price_path.query(doc)
        env.out.print("All prices (" + prices.size().string() + "): " +
          _format_results(prices))
      end

      // query_one: first book title
      try
        let first_title =
          json.JSONPathParser.compile("$.store.book[0].title")?
        match \exhaustive\ first_title.query_one(doc)
        | let title: json.JSONValue =>
          env.out.print("First title: " + json.JSONPrinter.print(title))
        | json.JSONNotFound =>
          env.out.print("No title found")
        end
      end

      // Union selectors: first two books
      try
        let first_two =
          json.JSONPathParser.compile("$.store.book[0,1].title")?
        let titles = first_two.query(doc)
        env.out.print("First two titles: " + _format_results(titles))
      end

      // Slice: first two books via slice
      try
        let slice =
          json.JSONPathParser.compile("$.store.book[:2].title")?
        let titles = slice.query(doc)
        env.out.print("Slice [:2] titles: " + _format_results(titles))
      end

      // Slice with step: every other book
      try
        let step_slice =
          json.JSONPathParser.compile("$.store.book[::2].title")?
        let titles = step_slice.query(doc)
        env.out.print("Slice [::2] titles: " + _format_results(titles))
      end

      // Slice with negative step: books in reverse
      try
        let rev_slice =
          json.JSONPathParser.compile("$.store.book[::-1].title")?
        let titles = rev_slice.query(doc)
        env.out.print("Slice [::-1] titles: " + _format_results(titles))
      end

    | let err: json.JSONParseError =>
      env.out.print("JSON parse error: " + err.string())
    end

  fun _jsonpath_filter_example(env: Env) =>
    let source =
      """
      {
        "store": {
          "book": [
            {"title": "Sayings",
             "author": "Rees", "price": 8.95},
            {"title": "Sword",
             "author": "Waugh", "price": 12.99},
            {"title": "Moby Dick",
             "author": "Melville", "price": 8.99}
          ],
          "bicycle": {"color": "red", "price": 399}
        }
      }
      """

    match \exhaustive\ json.JSONParser.parse(source)
    | let doc: json.JSONValue =>
      // Comparison filter: books under $10
      try
        let cheap =
          json.JSONPathParser.compile("$.store.book[?@.price < 10]")?
        let results = cheap.query(doc)
        env.out.print("Books under $10 (" + results.size().string() + "):")
        for book in results.values() do
          env.out.print("  " + json.JSONPrinter.print(book))
        end
      end

      // Existence filter: books that have an author
      try
        let has_author =
          json.JSONPathParser.compile("$.store.book[?@.author]")?
        let results = has_author.query(doc)
        env.out.print("Books with author: " + results.size().string())
      end

      // Logical combination: cheap books by specific author
      try
        let combined =
          json.JSONPathParser.compile(
            "$.store.book[?@.price < 10 && @.author == 'Rees']")?
        let results = combined.query(doc)
        env.out.print("Cheap books by Rees: " + _format_results(results))
      end

      // String comparison
      try
        let alpha =
          json.JSONPathParser.compile(
            "$.store.book[?@.author >= 'N'].title")?
        let results = alpha.query(doc)
        env.out.print("Authors >= 'N': " + _format_results(results))
      end

    | let err: json.JSONParseError =>
      env.out.print("JSON parse error: " + err.string())
    end

  fun _jsonpath_function_example(env: Env) =>
    let source =
      """
      {
        "users": [
          {"name": "Alice", "role": "admin",
           "tags": ["a", "b"]},
          {"name": "Bob", "role": "user",
           "tags": ["c"]},
          {"name": "Carol", "role": "admin",
           "tags": ["d", "e", "f"]}
        ]
      }
      """

    match \exhaustive\ json.JSONParser.parse(source)
    | let doc: json.JSONValue =>
      // match(): full-string I-Regexp match
      try
        let admins =
          json.JSONPathParser.compile("""$.users[?match(@.role, "admin")]""")?
        let results = admins.query(doc)
        env.out.print("Admins (match): " + _format_results(results))
      end

      // search(): substring I-Regexp search
      try
        let has_a =
          json.JSONPathParser.compile("""$.users[?search(@.name, "a")]""")?
        let results = has_a.query(doc)
        env.out.print("Names containing 'a' (search): " +
          results.size().string())
      end

      // length(): filter by string length
      try
        let short =
          json.JSONPathParser.compile("$.users[?length(@.name) <= 3]")?
        let results = short.query(doc)
        env.out.print("Short names (length <= 3): " +
          _format_results(results))
      end

      // count(): filter by array size
      try
        let multi =
          json.JSONPathParser.compile("$.users[?count(@.tags[*]) > 1]")?
        let results = multi.query(doc)
        env.out.print("Multiple tags (count > 1): " +
          results.size().string())
      end

    | let err: json.JSONParseError =>
      env.out.print("JSON parse error: " + err.string())
    end

  fun _format_results(results: Array[json.JSONValue] val): String =>
    let buf = recover iso String end
    buf.push('[')
    var first = true
    for v in results.values() do
      if not first then buf.append(", ") end
      first = false
      buf.append(json.JSONPrinter.print(v))
    end
    buf.push(']')
    consume buf

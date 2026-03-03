use json = "json"

actor Main
  new create(env: Env) =>
    env.out.print("=== Building JSON ===")
    _build_example(env)
    env.out.print("")

    env.out.print("=== Parsing JSON ===")
    _parse_example(env)
    env.out.print("")

    env.out.print("=== Navigation (JsonNav) ===")
    _nav_example(env)
    env.out.print("")

    env.out.print("=== Lenses (JsonLens) ===")
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
    let doc = json.JsonObject
      .update("name", "Alice")
      .update("age", I64(30))
      .update("active", true)
      .update("tags", json.JsonArray
        .push("admin")
        .push("developer"))
      .update("address", json.JsonObject
        .update("city", "Portland")
        .update("state", "OR"))

    env.out.print("Compact: " + doc.string())
    env.out.print("Pretty:")
    env.out.print(doc.pretty_string())

  fun _parse_example(env: Env) =>
    let source =
      """
      {"users":[{"name":"Bob","age":25},{"name":"Carol","age":35}],"count":2}
      """

    match \exhaustive\ json.JsonParser.parse(source)
    | let j: json.JsonValue =>
      env.out.print("Parsed successfully")
      match j
      | let obj: json.JsonObject => env.out.print("Root is object with "
        + obj.size().string() + " keys")
      end
    | let err: json.JsonParseError =>
      env.out.print("Parse error: " + err.string())
    end

  fun _nav_example(env: Env) =>
    let source =
      """
      {"users":[{"name":"Bob","age":25},{"name":"Carol","age":35}],"count":2}
      """

    match \exhaustive\ json.JsonParser.parse(source)
    | let j: json.JsonValue =>
      let nav = json.JsonNav(j)
      try
        let first_name = nav("users")(USize(0))("name").as_string()?
        let first_age = nav("users")(USize(0))("age").as_i64()?
        env.out.print("First user: " + first_name
          + ", age " + first_age.string())
      else
        env.out.print("Navigation failed")
      end

      try
        let count = nav("count").as_i64()?
        env.out.print("Count: " + count.string())
      end

      // JsonNotFound propagation — no crash, just JsonNotFound at the end
      let missing = nav("nonexistent")("deep")("path")
      env.out.print("Missing path found? " + missing.found().string())

    | let err: json.JsonParseError =>
      env.out.print("Parse error: " + err.string())
    end

  fun _lens_example(env: Env) =>
    let source =
      """
      {"config":{"database":{"host":"localhost","port":5432},"debug":false}}
      """

    match \exhaustive\ json.JsonParser.parse(source)
    | let j: json.JsonValue =>
      // Read via lens
      let host_lens = json.JsonLens("config")("database")("host")
      match \exhaustive\ host_lens.get(j)
      | let host: json.JsonValue =>
        env.out.print("Host: " + host.string())
      | json.JsonNotFound =>
        env.out.print("Host not found")
      end

      // Composed lens
      let db_lens = json.JsonLens("config")("database")
      let port_lens = db_lens.compose(json.JsonLens("port"))
      match \exhaustive\ port_lens.get(j)
      | let port: json.JsonValue =>
        env.out.print("Port: " + port.string())
      | json.JsonNotFound =>
        env.out.print("Port not found")
      end

    | let err: json.JsonParseError =>
      env.out.print("Parse error: " + err.string())
    end

  fun _lens_modify_example(env: Env) =>
    let source =
      """
      {"config":{"database":{"host":"localhost","port":5432},"debug":false}}
      """

    match \exhaustive\ json.JsonParser.parse(source)
    | let j: json.JsonValue =>
      // Modify a deeply nested value
      let host_lens = json.JsonLens("config")("database")("host")
      match \exhaustive\ host_lens.set(j, "prod.example.com")
      | let updated: json.JsonValue =>
        match updated
        | let obj: json.JsonObject =>
          env.out.print("After host change:")
          env.out.print(obj.pretty_string())
        end
      | json.JsonNotFound =>
        env.out.print("Could not set host — path not found")
      end

      // Remove a value
      let debug_lens = json.JsonLens("config")("debug")
      match \exhaustive\ debug_lens.remove(j)
      | let updated: json.JsonValue =>
        match updated
        | let obj: json.JsonObject =>
          env.out.print("After removing debug:")
          env.out.print(obj.pretty_string())
        end
      | json.JsonNotFound =>
        env.out.print("Could not remove debug — path not found")
      end

    | let err: json.JsonParseError =>
      env.out.print("Parse error: " + err.string())
    end

  fun _jsonpath_example(env: Env) =>
    let source =
      """
      {"store":{"book":[{"title":"Sayings","author":"Rees","price":8.95},{"title":"Sword","author":"Waugh","price":12.99},{"title":"Moby Dick","author":"Melville","price":8.99}],"bicycle":{"color":"red","price":399}}}
      """

    match \exhaustive\ json.JsonParser.parse(source)
    | let doc: json.JsonValue =>
      // Wildcard: all authors
      match \exhaustive\ json.JsonPathParser.parse("$.store.book[*].author")
      | let path: json.JsonPath =>
        let authors = path.query(doc)
        env.out.print("Authors: " + _format_results(authors))
      | let err: json.JsonPathParseError =>
        env.out.print("Path error: " + err.string())
      end

      // Recursive descent: all prices in the store
      try
        let price_path = json.JsonPathParser.compile("$.store..price")?
        let prices = price_path.query(doc)
        env.out.print("All prices (" + prices.size().string() + "): "
          + _format_results(prices))
      end

      // query_one: first book title
      try
        let first_title =
          json.JsonPathParser.compile("$.store.book[0].title")?
        match \exhaustive\ first_title.query_one(doc)
        | let title: json.JsonValue =>
          env.out.print("First title: " + title.string())
        | json.JsonNotFound =>
          env.out.print("No title found")
        end
      end

      // Union selectors: first two books
      try
        let first_two =
          json.JsonPathParser.compile("$.store.book[0,1].title")?
        let titles = first_two.query(doc)
        env.out.print("First two titles: " + _format_results(titles))
      end

      // Slice: first two books via slice
      try
        let slice =
          json.JsonPathParser.compile("$.store.book[:2].title")?
        let titles = slice.query(doc)
        env.out.print("Slice [:2] titles: " + _format_results(titles))
      end

      // Slice with step: every other book
      try
        let step_slice =
          json.JsonPathParser.compile("$.store.book[::2].title")?
        let titles = step_slice.query(doc)
        env.out.print("Slice [::2] titles: " + _format_results(titles))
      end

      // Slice with negative step: books in reverse
      try
        let rev_slice =
          json.JsonPathParser.compile("$.store.book[::-1].title")?
        let titles = rev_slice.query(doc)
        env.out.print("Slice [::-1] titles: " + _format_results(titles))
      end

    | let err: json.JsonParseError =>
      env.out.print("JSON parse error: " + err.string())
    end

  fun _jsonpath_filter_example(env: Env) =>
    let source =
      """
      {"store":{"book":[{"title":"Sayings","author":"Rees","price":8.95},{"title":"Sword","author":"Waugh","price":12.99},{"title":"Moby Dick","author":"Melville","price":8.99}],"bicycle":{"color":"red","price":399}}}
      """

    match \exhaustive\ json.JsonParser.parse(source)
    | let doc: json.JsonValue =>
      // Comparison filter: books under $10
      try
        let cheap =
          json.JsonPathParser.compile("$.store.book[?@.price < 10]")?
        let results = cheap.query(doc)
        env.out.print("Books under $10 (" + results.size().string() + "):")
        for book in results.values() do
          env.out.print("  " + book.string())
        end
      end

      // Existence filter: books that have an author
      try
        let has_author =
          json.JsonPathParser.compile("$.store.book[?@.author]")?
        let results = has_author.query(doc)
        env.out.print("Books with author: " + results.size().string())
      end

      // Logical combination: cheap books by specific author
      try
        let combined = json.JsonPathParser.compile(
          "$.store.book[?@.price < 10 && @.author == 'Rees']")?
        let results = combined.query(doc)
        env.out.print("Cheap books by Rees: " + _format_results(results))
      end

      // String comparison
      try
        let alpha = json.JsonPathParser.compile(
          "$.store.book[?@.author >= 'N'].title")?
        let results = alpha.query(doc)
        env.out.print("Authors >= 'N': " + _format_results(results))
      end

    | let err: json.JsonParseError =>
      env.out.print("JSON parse error: " + err.string())
    end

  fun _jsonpath_function_example(env: Env) =>
    let source =
      """
      {"users":[{"name":"Alice","role":"admin","tags":["a","b"]},{"name":"Bob","role":"user","tags":["c"]},{"name":"Carol","role":"admin","tags":["d","e","f"]}]}
      """

    match \exhaustive\ json.JsonParser.parse(source)
    | let doc: json.JsonValue =>
      // match(): full-string I-Regexp match
      try
        let admins =
          json.JsonPathParser.compile("""$.users[?match(@.role, "admin")]""")?
        let results = admins.query(doc)
        env.out.print("Admins (match): " + _format_results(results))
      end

      // search(): substring I-Regexp search
      try
        let has_a =
          json.JsonPathParser.compile("""$.users[?search(@.name, "a")]""")?
        let results = has_a.query(doc)
        env.out.print("Names containing 'a' (search): "
          + results.size().string())
      end

      // length(): filter by string length
      try
        let short =
          json.JsonPathParser.compile("$.users[?length(@.name) <= 3]")?
        let results = short.query(doc)
        env.out.print("Short names (length <= 3): "
          + _format_results(results))
      end

      // count(): filter by array size
      try
        let multi =
          json.JsonPathParser.compile("$.users[?count(@.tags[*]) > 1]")?
        let results = multi.query(doc)
        env.out.print("Multiple tags (count > 1): "
          + results.size().string())
      end

    | let err: json.JsonParseError =>
      env.out.print("JSON parse error: " + err.string())
    end

  fun _format_results(results: Array[json.JsonValue] val): String =>
    let buf = recover iso String end
    buf.push('[')
    var first = true
    for v in results.values() do
      if not first then buf.append(", ") end
      first = false
      buf.append(v.string())
    end
    buf.push(']')
    consume buf

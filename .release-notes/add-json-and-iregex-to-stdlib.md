## Add json and iregex packages to the standard library

Two new packages have been added to the standard library per RFC 0081.

The `json` package provides immutable JSON values backed by persistent collections. It includes `JsonParser` for parsing, `JsonNav` for chained read access, `JsonLens` for composable get/set/remove paths, and `JsonPath` for RFC 9535 string-based queries with filters and function extensions.

```pony
use json = "json"

// Build
let doc = json.JsonObject
  .update("name", "Alice")
  .update("age", I64(30))

// Parse
match json.JsonParser.parse(source)
| let j: json.JsonValue => // ...
| let err: json.JsonParseError => // ...
end

// Navigate
let nav = json.JsonNav(doc)
try nav("name").as_string()? end

// Query
try
  let path = json.JsonPathParser.compile("$.users[?@.age > 21]")?
  let results = path.query(doc)
end
```

The `iregex` package provides I-Regexp (RFC 9485) pattern matching — a deliberately constrained regular expression syntax designed for interoperability. The `json` package uses it internally for JSONPath `match()` and `search()` filter functions, but it is also available as a standalone package.

```pony
use iregex = "iregex"

match iregex.IRegexpCompiler.parse("[a-z]+")
| let re: iregex.IRegexp =>
  re.is_match("hello")   // true — full-string match
  re.search("abc123def") // true — substring match
| let err: iregex.IRegexpParseError =>
  // handle error
  None
end
```

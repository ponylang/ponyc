"""
# json-ng

JSON library for Pony. All JSON values are `val` — construction
uses chained method calls that return new values with structural sharing
via persistent collections. Three access patterns are available for
reading and modifying JSON structures, from simple one-shot lookups to
composable paths to string-based multi-match queries.

## Building JSON

`JsonObject` and `JsonArray` are constructed via chained method calls.
Each call returns a new value; the original is unchanged:

```pony
use json = "json"

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
```

Values in the `JsonValue` union — `JsonObject`, `JsonArray`, `String`,
`I64`, `F64`, `Bool`, and `None` — can be stored in objects and arrays.
JSON null is Pony's `None`.

## Parsing JSON

`JsonParser.parse()` returns errors as data — no exceptions to catch:

```pony
match json.JsonParser.parse(source)
| let j: json.JsonValue =>
  // j is the parsed document (object, array, or scalar)
  match j
  | let obj: json.JsonObject =>
    env.out.print("Root is object with " + obj.size().string() + " keys")
  end
| let err: json.JsonParseError =>
  env.out.print("Error at offset " + err.offset.string() + ": "
    + err.message)
end
```

## Reading Values: JsonNav

`JsonNav` wraps a value for chained read-only access. If any step
in the chain fails (missing key, out-of-bounds index, type mismatch),
`JsonNotFound` propagates silently through the rest of the chain — no
partial failures or exceptions:

```pony
let nav = json.JsonNav(doc)

// Chained access — returns the value or JsonNotFound
try
  let city = nav("address")("city").as_string()?
  env.out.print("City: " + city)
end

// JsonNotFound propagates — no crash, just JsonNotFound at the end
let missing = nav("nonexistent")("deep")("path")
if not missing.found() then
  env.out.print("Path not found")
end
```

Terminal extractors — `as_string()`, `as_i64()`, `as_f64()`,
`as_bool()`, `as_null()`, `as_object()`, `as_array()` — unwrap the
value or raise if the type doesn't match or the nav holds JsonNotFound.

## Composable Paths: JsonLens

`JsonLens` describes a reusable path (not tied to a specific document).
It supports reading, updating, and removing values. `compose()` chains
two lenses sequentially; `or_else()` tries a primary lens and falls
back to an alternative:

```pony
// Define a reusable path
let host_lens = json.JsonLens("config")("database")("host")

// Read
match host_lens.get(doc)
| let host: json.JsonValue => env.out.print("Host: " + host.string())
| json.JsonNotFound => env.out.print("no host configured")
end

// Update — returns a new document with the value changed
match host_lens.set(doc, "prod.example.com")
| let updated: json.JsonValue =>
  // updated is a new doc; original doc is unchanged
  None
| json.JsonNotFound => env.out.print("path doesn't exist")
end

// Remove a key
let debug_lens = json.JsonLens("config")("debug")
match debug_lens.remove(doc)
| let updated: json.JsonValue => None // debug key removed
| json.JsonNotFound => None // path didn't exist
end

// Compose two lenses
let db_lens = json.JsonLens("config")("database")
let port_lens = db_lens.compose(json.JsonLens("port"))

// Fallback: try primary, fall back to alternative
let fallback = json.JsonLens("primary_host")
  .or_else(json.JsonLens("fallback_host"))
```

## String-Based Queries: JsonPath

`JsonPath` implements a subset of RFC 9535 — string-based query
expressions that can match multiple values via wildcards, recursive
descent, and slicing. Parse a path string once, then apply it to any
number of documents:

```pony
// Parse returns errors as data (consistent with JsonParser)
match json.JsonPathParser.parse("$.store.book[*].author")
| let path: json.JsonPath =>
  let authors = path.query(doc) // Array[JsonValue] val
  for author in authors.values() do
    env.out.print(author.string())
  end
| let err: json.JsonPathParseError =>
  env.out.print(err.string())
end

// compile() raises on invalid input — use for known-valid paths
try
  let prices = json.JsonPathParser.compile("$.store..price")?
  let results = prices.query(doc)
end
```

Supported JSONPath syntax:

* `$.key` or `$['key']` — child by name
* `$[0]` or `$[-1]` — array index (negative counts from end)
* `$[*]` or `$.*` — wildcard (all children)
* `$..key` or `$..*` — recursive descent
* `$[0:3]` — slice (start inclusive, end exclusive)
* `$[::2]` or `$[::-1]` — slice with step (forward or reverse)
* `$[0,2,4]` — union (multiple indices or names)
* `$[?@.price < 10]` — filter by comparison
* `$[?@.author]` — filter by existence (member present)
* `$[?@.a > 1 && @.b < 2]` — logical AND, OR (`||`), NOT (`!`)
* `$[?@.type == $.default]` — absolute query (`$`) in filters
* `$[?match(@.name, "[A-Z].*")]` — function extensions (`length`, `count`,
  `match`, `search`, `value`)
* `query_one()` — convenience returning first match or `JsonNotFound`

## Serialization

`JsonObject` and `JsonArray` implement `Stringable`. Compact output
uses `string()`; indented output uses `pretty_string()`:

```pony
let obj = json.JsonObject
  .update("a", I64(1))
  .update("b", json.JsonArray.push(I64(2)).push(I64(3)))

env.out.print(obj.string())
// {"a":1,"b":[2,3]}

env.out.print(obj.pretty_string())
// {
//   "a": 1,
//   "b": [
//     2,
//     3
//   ]
// }

// Custom indent string (default is two spaces)
env.out.print(obj.pretty_string("\t"))
```

## Choosing an Access Pattern

* **`JsonNav`** — one-shot chained access. Read-only. Best for
  "grab this one value." Wraps a specific document; JsonNotFound propagates
  through chains.

* **`JsonLens`** — reusable path with get/set/remove. Best for
  "define a path once, apply it to many documents." Supports
  composition (`compose`) and fallbacks (`or_else`). Not tied to a
  specific document.

* **`JsonPath`** — string-based query language (RFC 9535 subset). Best
  for "find all values matching a pattern." Supports wildcards,
  recursive descent, and slicing. Returns arrays of results.

Start with `JsonNav` for simple reads. Move to `JsonLens` when you
need to modify values or reuse paths. Use `JsonPath` when you need
multi-match queries, wildcard selection, or user-provided path strings.

## Token Parser

`JsonTokenParser` is a streaming alternative to `JsonParser`. Instead
of building a complete document tree, it emits tokens to a callback
as they are encountered. Use this when you need to process large
documents without materializing the full tree, or when you need custom
processing logic:

```pony
let parser = json.JsonTokenParser(
  object is json.JsonTokenNotify
    fun ref apply(parser': json.JsonTokenParser, token: json.JsonToken) =>
      match token
      | json.JsonTokenKey => env.out.print("Key: " + parser'.last_string)
      | json.JsonTokenString => env.out.print("String: " + parser'.last_string)
      | json.JsonTokenNumber =>
        match parser'.last_number
        | let n: I64 => env.out.print("Int: " + n.string())
        | let f: F64 => env.out.print("Float: " + f.string())
        end
      | json.JsonTokenObjectStart => env.out.print("{")
      | json.JsonTokenObjectEnd => env.out.print("}")
      | json.JsonTokenArrayStart => env.out.print("[")
      | json.JsonTokenArrayEnd => env.out.print("]")
      end
  end)
try parser.parse(source)? end
```

For most use cases, `JsonParser.parse()` is simpler and sufficient.

"""

use "collections/persistent"

type JsonValue is (JsonObject | JsonArray | String | I64 | F64 | Bool | None)

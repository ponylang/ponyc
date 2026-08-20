"""
# json

JSON library for Pony. All JSON values are `val` — construction
uses chained method calls that return new values with structural sharing
via persistent collections.

The two ends of the library are `JSONParser` (JSON text → `JSONValue`) and
`JSONPrinter` (`JSONValue` → JSON text); `JSONTokenParser` is a third, streaming
path from text that never builds the whole tree. On top of `JSONValue`, three
access patterns are available for reading and modifying structures, from simple
one-shot lookups to composable paths to string-based multi-match queries.

## Parsing JSON

`JSONParser.parse()` returns errors as data — no exceptions to catch:

```pony
match json.JSONParser.parse(source)
| let j: json.JSONValue =>
  // j is the parsed document (object, array, or scalar)
  match j
  | let obj: json.JSONObject =>
    env.out.print("Root is object with " + obj.size().string() + " keys")
  end
| let err: json.JSONParseError =>
  env.out.print("Error at offset " + err.offset.string() + ": "
    + err.message)
end
```

## Serializing JSON

`JSONPrinter` is the dual of `JSONParser`: it encodes any `JSONValue` —
objects, arrays, and scalars alike — into valid JSON. `print()` produces
compact output; `pretty()` produces indented output:

```pony
let doc = json.JSONObject
  .update("a", I64(1))
  .update("b", json.JSONArray.push(I64(2)).push(I64(3)))

env.out.print(json.JSONPrinter.print(doc))
// {"a":1,"b":[2,3]}

env.out.print(json.JSONPrinter.pretty(doc))
// {
//   "a": 1,
//   "b": [
//     2,
//     3
//   ]
// }

// Custom indent string (default is two spaces)
env.out.print(json.JSONPrinter.pretty(doc, "\t"))

// Scalars and JSON null serialize correctly too
env.out.print(json.JSONPrinter.print(None))  // null
env.out.print(json.JSONPrinter.print(true))  // true
```

This is also how you serialize Pony data as JSON: build a `JSONValue`, then
hand it to `JSONPrinter.print()`.

`JSONPrinter.print` is the general entry point and the form to reach for
first: it is the only one that serializes scalars (`String`, `I64`,
`F64`, `Bool`) and JSON null (`None`). For convenience, `JSONObject` and
`JSONArray` also expose `print()` and `pretty_print()` directly,
equivalent to passing them to `JSONPrinter`:

```pony
env.out.print(doc.print())         // same as JSONPrinter.print(doc)
env.out.print(doc.pretty_print())  // same as JSONPrinter.pretty(doc)
```

## Building JSON

`JSONObject` and `JSONArray` are constructed via chained method calls.
Each call returns a new value; the original is unchanged:

```pony
use json = "json"

let doc = json.JSONObject
  .update("name", "Alice")
  .update("age", I64(30))
  .update("active", true)
  .update("tags", json.JSONArray
    .push("admin")
    .push("developer"))
  .update("address", json.JSONObject
    .update("city", "Portland")
    .update("state", "OR"))
```

Values in the `JSONValue` union — `JSONObject`, `JSONArray`, `String`,
`I64`, `F64`, `Bool`, and `None` — can be stored in objects and arrays.
JSON null is Pony's `None`.

## Reading Values: JSONNav

`JSONNav` wraps a value for chained read-only access. If any step
in the chain fails (missing key, out-of-bounds index, type mismatch),
`JSONNotFound` propagates silently through the rest of the chain — no
partial failures or exceptions:

```pony
let nav = json.JSONNav(doc)

// Chained access — returns the value or JSONNotFound
try
  let city = nav("address")("city").as_string()?
  env.out.print("City: " + city)
end

// JSONNotFound propagates — no crash, just JSONNotFound at the end
let missing = nav("nonexistent")("deep")("path")
if not missing.found() then
  env.out.print("Path not found")
end
```

Terminal extractors — `as_string()`, `as_i64()`, `as_f64()`,
`as_bool()`, `as_null()`, `as_object()`, `as_array()` — unwrap the
value or raise if the type doesn't match or the nav holds JSONNotFound.

## Composable Paths: JSONLens

`JSONLens` describes a reusable path (not tied to a specific document).
It supports reading, updating, and removing values. `compose()` chains
two lenses sequentially; `or_else()` tries a primary lens and falls
back to an alternative:

```pony
// Define a reusable path
let host_lens = json.JSONLens("config")("database")("host")

// Read
match host_lens.get(doc)
| let host: json.JSONValue =>
  env.out.print("Host: " + json.JSONPrinter.print(host))
| json.JSONNotFound => env.out.print("no host configured")
end

// Update — returns a new document with the value changed
match host_lens.set(doc, "prod.example.com")
| let updated: json.JSONValue =>
  // updated is a new doc; original doc is unchanged
  None
| json.JSONNotFound => env.out.print("path doesn't exist")
end

// Remove a key
let debug_lens = json.JSONLens("config")("debug")
match debug_lens.remove(doc)
| let updated: json.JSONValue => None // debug key removed
| json.JSONNotFound => None // path didn't exist
end

// Compose two lenses
let db_lens = json.JSONLens("config")("database")
let port_lens = db_lens.compose(json.JSONLens("port"))

// Fallback: try primary, fall back to alternative
let fallback = json.JSONLens("primary_host")
  .or_else(json.JSONLens("fallback_host"))
```

## String-Based Queries: JSONPath

`JSONPath` implements a subset of RFC 9535 — string-based query
expressions that can match multiple values via wildcards, recursive
descent, and slicing. Parse a path string once, then apply it to any
number of documents:

```pony
// Parse returns errors as data (consistent with JSONParser)
match json.JSONPathParser.parse("$.store.book[*].author")
| let path: json.JSONPath =>
  let authors = path.query(doc) // Array[JSONValue] val
  for author in authors.values() do
    env.out.print(json.JSONPrinter.print(author))
  end
| let err: json.JSONPathParseError =>
  env.out.print(err.string())
end

// compile() raises on invalid input — use for known-valid paths
try
  let prices = json.JSONPathParser.compile("$.store..price")?
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
* `query_one()` — convenience returning first match or `JSONNotFound`

## Choosing an Access Pattern

* **`JSONNav`** — one-shot chained access. Read-only. Best for
  "grab this one value." Wraps a specific document; JSONNotFound propagates
  through chains.

* **`JSONLens`** — reusable path with get/set/remove. Best for
  "define a path once, apply it to many documents." Supports
  composition (`compose`) and fallbacks (`or_else`). Not tied to a
  specific document.

* **`JSONPath`** — string-based query language (RFC 9535 subset). Best
  for "find all values matching a pattern." Supports wildcards,
  recursive descent, and slicing. Returns arrays of results.

Start with `JSONNav` for simple reads. Move to `JSONLens` when you
need to modify values or reuse paths. Use `JSONPath` when you need
multi-match queries, wildcard selection, or user-provided path strings.

## Streaming with JSONTokenParser

`JSONParser.parse()` needs the whole document in memory and builds the whole
tree. When JSON arrives in pieces — over a socket, or a file read in chunks — or
is too big to hold at once, `JSONTokenParser` streams it. Feed it bytes with
`feed()` and it pushes tokens (object start, a key, a value, and so on) to your
notifier as they complete, walking the structure to any depth without building a
tree. A value split across a chunk boundary is held and finished by the next
`feed()`. Each token carries its own value:

```pony
let parser = json.JSONTokenParser(
  object is json.JSONTokenNotify
    fun ref apply(p: json.JSONTokenParser, token: json.JSONToken) =>
      match token
      | let k: json.JSONTokenKey    => env.out.print("Key: " + k.value)
      | let s: json.JSONTokenString => env.out.print("String: " + s.value)
      | let n: json.JSONTokenNumber =>
        match n.value
        | let i: I64 => env.out.print("Int: " + i.string())
        | let f: F64 => env.out.print("Float: " + f.string())
        end
      | json.JSONTokenObjectStart => env.out.print("{")
      | json.JSONTokenObjectEnd => env.out.print("}")
      | json.JSONTokenArrayStart => env.out.print("[")
      | json.JSONTokenArrayEnd => env.out.print("]")
      | json.JSONTokenTrue | json.JSONTokenFalse | json.JSONTokenNull => None
      end
  end)
try
  parser.feed(chunk)?  // call once per chunk as bytes arrive
  parser.finish()?     // when no more bytes are coming
end
```

You control the memory. Process each token and drop it, and memory
stays flat no matter how big the document — the parser holds only the
container-depth stack, the one value it is mid-parse on, and the fed
bytes it has not yet consumed (feed in chunks and drain to keep that
last part small). To pull a few fields out of a large document, ignore
the tokens you don't want; there is no skip to learn, and a value you
never keep is never held. Always call `finish()` when the input ends:
it completes a trailing number (the one value with no self-delimiter),
and `incomplete()` then tells you whether the input ended mid-value.
For untrusted input, pass a `JSONParseLimits` to cap depth and the
length of a single string or number.

## Reassembling values from a token stream

When you do want a `JSONValue` — for one record, say, not the whole document —
`JSONReassembler` folds a run of tokens back into the same `JSONValue` a batch
parse would return. It is a `JSONTokenNotify`, so it plugs straight into the
parser:

```pony
let reassembler = json.JSONReassembler
let parser = json.JSONTokenParser(reassembler)
parser.feed(chunk)?
for value in reassembler.take_values().values() do
  // value : JSONValue — use it with JSONNav, JSONLens, JSONPath, JSONPrinter
end
```

Hand it every token and you have buffered the whole document; hand it one
record's tokens, take the value, and drop it, and memory stays flat. The choice
is yours.

## Choosing between the parsers

* **`JSONParser.parse()`** — the whole document is in memory and you want the
  whole tree. Simplest; reach for it first. It applies no resource limits, so it
  is for trusted input; for a document of unknown origin, use `JSONTokenParser`
  with a `JSONParseLimits`.
* **`JSONTokenParser` with your own notifier** — JSON arrives in pieces, or is
  too large to hold, or you want to react to values as they stream past without
  building a tree.
* **`JSONTokenParser` with `JSONReassembler`** — streaming input, but you want
  `JSONValue`s out. You decide which values to materialize, so you decide the
  memory cost.

"""

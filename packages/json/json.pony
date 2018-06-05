"""
# JSON Package

The `json` package provides the [JsonDoc](json-JsonDoc) class both as a container for a JSON
document and as means of parsing from and writing to [String](builtin-String).

## JSON Representation

JSON is represented in Pony as the following types:

* object - [JsonObject](json-JsonObject)
* array  - [JsonArray](json-JsonArray)
* string - [String](builtin-String)
* integer - [I64](builtin-I64)
* float   - [F64](builtin-F64)
* boolean - [Bool](builtin-Bool)
* null    - [None](builtin-None)

The collection types JsonObject and JsonArray can contain any other JSON
structures arbitrarily nested.

[JsonType](json-JsonType) is used to subsume all possible JSON types. It can
also be used to describe everything that can be serialized using this package.

## Parsing JSON

For getting JSON from a String into proper Pony data structures,
[JsonDoc.parse](json-JsonDoc#parse) needs to be used. This will populate the
public field `JsonDoc.data`, which is [None](builtin-None), if [parse](json-JsonDoc#parse) has
not been called yet.

Every call to [parse](json-JsonDoc#parse) overwrites the `data` field, so one
JsonDoc instance can be used to parse multiple JSON Strings one by one.

```pony
val doc = JsonDoc
// parsing
doc.parse(\"\"\"{"key":"value", "property: true, "array":[1, 2.5, false]}\"\"\")?

// extracting values from a JSON structure
val json: JsonObject  = doc.data as JsonObject
val key: String       = json.data("key")? as String
val property: Boolean = json.data("property")? as Bool
val array: JsonArray  = json.data("array")?
val first: I64        = array.data(0)? as I64
val second: F64       = array.data(1)? as F64
val last: Bool        = array.data(2)? as Bool
```

## Writing JSON

JSON is written using the [JsonDoc.string](json-JsonDoc#string) method. This
will serialize the contents of the `data` field to [String](builtin-String).

```pony
// building up the JSON data structure
let doc = JsonDoc
let obj = JsonObject
obj.data("key") = "value"
obj.data("property") = true
obj.data("array") = JsonArray.from_array([ as JsonType: I64(1); F64(2.5); false])
doc.data = obj

// writing to String
env.out.print(
  doc.string(where indent="  ", pretty_print=true)
)

```
"""

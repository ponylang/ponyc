## Add a streaming JSON parser

`JsonStreamParser` parses JSON incrementally: feed it bytes as they arrive with `feed()` and call `pull()` to take complete top-level values one at a time, without buffering the whole input first. It returns the same `JsonValue` as `JsonParser`, so streamed values work with `JsonNav`, `JsonLens`, `JsonPath`, and `JsonPrinter` unchanged.

```pony
use "json"

let parser = JsonStreamParser
parser.feed(chunk)
match parser.pull()
| let v: JsonValue      => // a complete top-level value
| JsonNeedMore          => // feed more bytes, then pull() again
| let e: JsonParseError => // malformed input
end
```

It handles a stream of top-level object/array values: a single document yields one value, while a sequence (object-per-line newline-delimited JSON, concatenated values, a socket delivering messages) yields each value as it completes, with no per-document setup. Each top-level value must be an object or array; a bare scalar (including a scalar NDJSON line) is not supported. `JsonNeedMore` is the explicit "incomplete, not yet malformed" outcome — a value split across a chunk boundary yields `JsonNeedMore` rather than an error, so feeding the rest later completes it. For untrusted input, `JsonStreamLimits` caps per-value nesting depth, string length, number length, and total size.

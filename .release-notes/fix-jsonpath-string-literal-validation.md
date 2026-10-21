## Fix JSONPathParser accepting invalid string literals

`JSONPathParser.parse` accepted JSONPath queries containing raw control characters (U+0000 through U+001F) in string literals, and accepted `\'` escapes in double-quoted strings and `\"` escapes in single-quoted strings. RFC 9535 section 2.3.1.1 requires raw control characters to be escaped and restricts each quote escape to its own quote type. These queries now return `JSONPathParseError`.

```pony
// These were accepted, now rejected:
JSONPathParser.parse("$['a\x01b']")  // raw control character
JSONPathParser.parse("$[\"\\'\"]")   // \' in a double-quoted string
JSONPathParser.parse("$['\\\"']")    // \" in a single-quoted string

// Escaped control characters remain valid:
JSONPathParser.parse("$['a\\u0001b']") // \u0001 is the escaped form
```

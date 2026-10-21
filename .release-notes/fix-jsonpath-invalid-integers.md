## Fix JSONPathParser accepting integers RFC 9535 does not allow

`JSONPathParser` accepted queries with negative zero (`$[-0]`), leading zeros (`$[01]`), and a lone minus sign (`$[-:]`) in index and slice positions. RFC 9535 section 2.3.3.1 defines `int = "0" / (["-"] DIGIT1 *DIGIT)`, so these are not well-formed and must be rejected. These queries now return `JSONPathParseError`.

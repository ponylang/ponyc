## Fix the start position reported for a JSON String or Key token

`JsonTokenParser` reports a `token_start()` and `token_end()` byte offset for each token it emits. For a `String` or `Key` token, `token_start()` now marks the opening `"`, so the reported span covers the whole quoted token. Previously it pointed one byte past the opening quote, so the span dropped the opening quote — inconsistent with every other token, which already reported its first byte.

## Fix `ParseFormURLEncoded` producing spurious pairs from empty `&`-separated sequences

`ParseFormURLEncoded` turned every `&`-delimited slice into a key-value pair, including empty slices. A leading, trailing, or doubled `&` produced a spurious `("", "")` pair: `"a=1&"` parsed to two pairs instead of one, and `"&&"` parsed to three empty pairs instead of zero. `URI.query_params()` had the same problem because it calls `ParseFormURLEncoded` directly.

Empty sequences are now skipped, matching the WHATWG URL Standard §5.1 step 3.1. A bare `=` still produces `("", "")` because it is not an empty sequence.

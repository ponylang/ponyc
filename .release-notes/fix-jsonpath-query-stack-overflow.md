## Fix crash from deeply nested JSONPath query strings

A crafted JSONPath query string could overflow the stack and crash the program with a segfault instead of parsing or evaluating cleanly. A path that nested filter parentheses, function calls, or `[?...]` filters tens of thousands of levels deep crashed while parsing; a path that chained `&&` or `||` hundreds of thousands of times parsed but crashed while evaluating. Because `JsonPath` is the documented way to apply user-supplied path strings, a small crafted string — well under a megabyte — was enough to take the process down.

These query strings are now handled safely. A long `&&`/`||` chain evaluates without crashing, limited only by available memory. A path that nests parentheses, function calls, or `[?...]` filters past a generous depth limit is rejected with a catchable `JsonPathParseError` rather than crashing — the limit sits far above the nesting any real query needs.

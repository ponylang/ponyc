## Fix CommandParser accepting malformed short option tokens

`CommandParser` accepted tokens like `-=x` and `-=` without returning a `SyntaxError`. The cli package grammar requires at least one letter after `-` for a short option (`-a`, `-abc`), but tokens starting with `-` followed by a non-letter, non-digit character were silently treated as positional arguments. These tokens now produce a `SyntaxError`.

```pony
// Before: no error returned
CommandParser(spec).parse(["app"; "-=x"])  // -> Command

// After: returns an error
CommandParser(spec).parse(["app"; "-=x"])  // -> SyntaxError
```
